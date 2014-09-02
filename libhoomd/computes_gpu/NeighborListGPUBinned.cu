/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2009-2014 The Regents of
the University of Michigan All rights reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: joaander

#include "NeighborListGPUBinned.cuh"
#include "TextureTools.h"

/*! \file NeighborListGPUBinned.cu
    \brief Defines GPU kernel code for O(N) neighbor list generation on the GPU
*/

//! Texture for reading d_cell_xyzf
scalar4_tex_t cell_xyzf_1d_tex;

//! Warp-centric scan
template<int NT>
struct warp_scan
    {
    #if __CUDA_ARCH__ >= 300
    enum { capacity = 0 }; // uses no shared memory
    #else
    enum { capacity = NT > 1 ? (2 * NT + 1) : 1};
    #endif

    __device__ static int Scan(int tid, unsigned char x, volatile unsigned char *shared, unsigned char* total)
        {
        #if __CUDA_ARCH__ >= 300
        // Kepler version
        unsigned int laneid;
        //This command gets the lane ID within the current warp
        asm("mov.u32 %0, %%laneid;" : "=r"(laneid));

        int first = laneid - tid;

        #pragma unroll
        for(int offset = 1; offset < NT; offset += offset)
            {
            int y = __shfl(x,(first + tid - offset) &(WARP_SIZE -1));
            if(tid >= offset) x += y;
            }

        // all threads get the total from the last thread in the cta
        *total = __shfl(x,first + NT - 1);

        // shift by one (exclusive scan)
        int y = __shfl(x,(first + tid - 1) &(WARP_SIZE-1));
        x = tid ? y : 0;

        #else // __CUDA_ARCH__ >= 300

        shared[tid] = x;
        int first = 0;
        // no syncthreads here (inside warp)

        for(int offset = 1; offset < NT; offset += offset)
            {
            if(tid >= offset)
                x = shared[first + tid - offset] + x;
            first = NT - first;
            shared[first + tid] = x;
            // no syncthreads here (inside warp)
            }
        *total = shared[first + NT - 1];

        // shift by one (exclusive scan)
        x = tid ? shared[first + tid - 1] : 0;
        #endif
        // no syncthreads here (inside warp)
        return x;
        }
    };

//! Kernel call for generating neighbor list on the GPU (shared memory version)
/*! \tparam flags Set bit 1 to enable body filtering. Set bit 2 to enable diameter filtering.
    \param d_nlist Neighbor list data structure to write
    \param d_n_neigh Number of neighbors to write
    \param d_last_updated_pos Particle positions at this update are written to this array
    \param d_conditions Conditions array for writing overflow condition
    \param nli Indexer to access \a d_nlist
    \param d_pos Particle positions
    \param d_body Particle body indices
    \param d_diameter Particle diameters
    \param N Number of particles
    \param d_cell_size Number of particles in each cell
    \param d_cell_xyzf Cell contents (xyzf array from CellList with flag=type)
    \param d_cell_tdb Cell contents (tdb array from CellList with)
    \param d_cell_adj Cell adjacency list
    \param ci Cell indexer for indexing cells
    \param cli Cell list indexer for indexing into d_cell_xyzf
    \param cadji Adjacent cell indexer listing the 27 neighboring cells
    \param box Simulation box dimensions
    \param r_maxsq The maximum radius for which to include particles as neighbors, squared
    \param r_max The maximum radius for which to include particles as neighbors
    \param ghost_width Width of ghost cell layer

    \note optimized for Fermi
*/
template<unsigned char flags, int threads_per_particle>
__global__ void gpu_compute_nlist_binned_shared_kernel(unsigned int *d_nlist,
                                                    unsigned int *d_n_neigh,
                                                    Scalar4 *d_last_updated_pos,
                                                    unsigned int *d_conditions,
                                                    const unsigned int *d_Nmax,
                                                    const unsigned int *d_head_list,
                                                    const Scalar4 *d_pos,
                                                    const unsigned int *d_body,
                                                    const Scalar *d_diameter,
                                                    const unsigned int N,
                                                    const unsigned int *d_cell_size,
                                                    const Scalar4 *d_cell_xyzf,
                                                    const Scalar4 *d_cell_tdb,
                                                    const unsigned int *d_cell_adj,
                                                    const Index3D ci,
                                                    const Index2D cli,
                                                    const Index2D cadji,
                                                    const BoxDim box,
                                                    const Scalar *d_r_listsq,
                                                    const unsigned int ntypes,
                                                    const Scalar3 ghost_width)
    {
    bool filter_body = flags & 1;

    // cache the r_listsq parameters into shared memory
    Index2D typpair_idx(ntypes);
    const unsigned int num_typ_parameters = typpair_idx.getNumElements();

    // shared data for per type pair parameters
    extern __shared__ unsigned char s_data[];
    
    // pointer for the r_listsq data
    Scalar *s_r_listsq = (Scalar *)(&s_data[0]);
    unsigned int *s_Nmax = (unsigned int *)(&s_data[sizeof(unsigned int)*num_typ_parameters]);

    // load in the per type pair r_list
    for (unsigned int cur_offset = 0; cur_offset < num_typ_parameters; cur_offset += blockDim.x)
        {
        if (cur_offset + threadIdx.x < num_typ_parameters)
            {
            s_r_listsq[cur_offset + threadIdx.x] = d_r_listsq[cur_offset + threadIdx.x];
            }
        if (cur_offset + threadIdx.x < ntypes)
            {
            s_Nmax[cur_offset + threadIdx.x] = d_Nmax[cur_offset + threadIdx.x];
            }
        }
    __syncthreads();

    // each set of threads_per_particle threads is going to compute the neighbor list for a single particle
    int my_pidx;
    if (gridDim.y > 1)
        {
        // fermi workaround
        my_pidx = (blockIdx.x + blockIdx.y*65535) * (blockDim.x/threads_per_particle) + threadIdx.x/threads_per_particle;
        }
    else
        {
        my_pidx = blockIdx.x * (blockDim.x/threads_per_particle) + threadIdx.x/threads_per_particle;
        }

    // return early if out of bounds
    if (my_pidx >= N) return;

    // first, determine which bin this particle belongs to
    Scalar4 my_postype = d_pos[my_pidx];
    Scalar3 my_pos = make_scalar3(my_postype.x, my_postype.y, my_postype.z);

    unsigned int my_type = __scalar_as_int(my_postype.w);
    unsigned int my_body = d_body[my_pidx];

    Scalar3 f = box.makeFraction(my_pos, ghost_width);

    // find the bin each particle belongs in
    int ib = (int)(f.x * ci.getW());
    int jb = (int)(f.y * ci.getH());
    int kb = (int)(f.z * ci.getD());

    uchar3 periodic = box.getPeriodic();

    // need to handle the case where the particle is exactly at the box hi
    if (ib == ci.getW() && periodic.x)
        ib = 0;
    if (jb == ci.getH() && periodic.y)
        jb = 0;
    if (kb == ci.getD() && periodic.z)
        kb = 0;

    int my_cell = ci(ib,jb,kb);

    // pointer into shared memory for calculation (volatile is required, since we are doing warp-centric)
    volatile unsigned char *sh =(unsigned char *)&s_data[sizeof(Scalar)*num_typ_parameters+sizeof(unsigned int)*ntypes];

    // index of current neighbor
    unsigned int cur_adj = 0;

    // current cell
    unsigned int neigh_cell = d_cell_adj[cadji(cur_adj, my_cell)];

    // size of current cell
    unsigned int neigh_size = d_cell_size[neigh_cell];

    // offset of cta in shared memory
    int cta_offs = (threadIdx.x/threads_per_particle)*warp_scan<threads_per_particle>::capacity;

    // current index in cell
    int cur_offset = threadIdx.x % threads_per_particle;

    bool done = false;

    // total number of neighbors
    unsigned int nneigh = 0;

    while (! done)
        {
        // initalize with default
        unsigned int neighbor;
        unsigned char has_neighbor = 0;

        // advance neighbor cell
        while (cur_offset >= neigh_size && !done )
            {
            cur_offset -= neigh_size;
            cur_adj++;
            if (cur_adj < cadji.getW())
                {
                neigh_cell = d_cell_adj[cadji(cur_adj, my_cell)];
                neigh_size = d_cell_size[neigh_cell];
                }
            else
                // we are past the end of the cell neighbors
                done = true;
            }

        // if the first thread in the cta has no work, terminate the loop
        if (done && !(threadIdx.x % threads_per_particle)) break;

        if (!done)
            {
            Scalar4 cur_xyzf = texFetchScalar4(d_cell_xyzf, cell_xyzf_1d_tex, cli(cur_offset, neigh_cell));

            Scalar4 cur_tdb = d_cell_tdb[cli(cur_offset, neigh_cell)];

            // advance cur_offset
            cur_offset += threads_per_particle;

            unsigned int neigh_type = __scalar_as_int(cur_tdb.x);
            unsigned int neigh_body = __scalar_as_int(cur_tdb.z);

            Scalar3 neigh_pos = make_scalar3(cur_xyzf.x,
                                           cur_xyzf.y,
                                           cur_xyzf.z);
            int cur_neigh = __scalar_as_int(cur_xyzf.w);

            // compute the distance between the two particles
            Scalar3 dx = my_pos - neigh_pos;

            // wrap the periodic boundary conditions
            dx = box.minImage(dx);

            // compute dr squared
            Scalar drsq = dot(dx,dx);

            bool excluded = (my_pidx == cur_neigh);

            if (filter_body && my_body != 0xffffffff)
                excluded = excluded | (my_body == neigh_body);

            // store result in shared memory
            if (drsq <= s_r_listsq[typpair_idx(my_type,neigh_type)] && !excluded)
                {
                neighbor = cur_neigh;
                has_neighbor = 1;
                }
            }

        // no syncthreads here, we assume threads_per_particle < warp size

        // scan over flags
        unsigned char n;
        int k = warp_scan<threads_per_particle>::Scan(threadIdx.x % threads_per_particle,
            has_neighbor, &sh[cta_offs], &n);

        if (has_neighbor && nneigh + k < s_Nmax[my_type])
            d_nlist[d_head_list[my_pidx] + nneigh + k] = neighbor;

        // increment total neighbor count
        nneigh += n;
        } // end while

    if (threadIdx.x % threads_per_particle == 0)
        {
        // flag if we need to grow the neighbor list
        if (nneigh >= s_Nmax[my_type])
            atomicMax(&d_conditions[my_type], nneigh);

        d_n_neigh[my_pidx] = nneigh;
        d_last_updated_pos[my_pidx] = my_postype;
        }
    }

//! determine maximum possible block size
template<typename T>
int get_max_block_size(T func)
    {
    cudaFuncAttributes attr;
    cudaFuncGetAttributes(&attr, (const void*)func);
    int max_threads = attr.maxThreadsPerBlock;
    // number of threads has to be multiple of warp size
    max_threads -= max_threads % max_threads_per_particle;
    return max_threads;
    }

void gpu_nlist_binned_bind_texture(const Scalar4 *d_cell_xyzf, unsigned int n_elements)
    {
    // bind the position texture
    cell_xyzf_1d_tex.normalized = false;
    cell_xyzf_1d_tex.filterMode = cudaFilterModePoint;
    cudaBindTexture(0, cell_xyzf_1d_tex, d_cell_xyzf, sizeof(Scalar4)*n_elements);
    }

//! recursive template to launch neighborlist with given template parameters
/* \tparam cur_tpp Number of threads per particle (assumed to be power of two) */
template<int cur_tpp>
inline void launcher(unsigned int *d_nlist,
              unsigned int *d_n_neigh,
              Scalar4 *d_last_updated_pos,
              unsigned int *d_conditions,
              const unsigned int *d_Nmax,
              const unsigned int *d_head_list,
              const Scalar4 *d_pos,
              const unsigned int *d_body,
              const Scalar *d_diameter,
              const unsigned int N,
              const unsigned int *d_cell_size,
              const Scalar4 *d_cell_xyzf,
              const Scalar4 *d_cell_tdb,
              const unsigned int *d_cell_adj,
              const Index3D ci,
              const Index2D cli,
              const Index2D cadji,
              const BoxDim box,
              const Scalar *d_r_listsq,
              const unsigned int ntypes,
              const Scalar3 ghost_width,
              const unsigned int compute_capability,
              unsigned int tpp,
              bool filter_body,
              unsigned int block_size)
    {
    // shared memory = r_listsq + Nmax + stuff needed for neighborlist (computed below)
    Index2D typpair_idx(ntypes);
    unsigned int shared_size = sizeof(Scalar)*typpair_idx.getNumElements() + sizeof(unsigned int)*ntypes;

    if (tpp == cur_tpp && cur_tpp != 0)
        {
        if (!filter_body)
            {
            static unsigned int max_block_size = UINT_MAX;
            if (max_block_size == UINT_MAX)
                max_block_size = get_max_block_size(gpu_compute_nlist_binned_shared_kernel<0,cur_tpp>);
            if (compute_capability < 35) gpu_nlist_binned_bind_texture(d_cell_xyzf, cli.getNumElements());

            block_size = block_size < max_block_size ? block_size : max_block_size;
            dim3 grid(N / (block_size/tpp) + 1);
            if (compute_capability < 30 && grid.x > 65535)
                {
                grid.y = grid.x/65535 + 1;
                grid.x = 65535;
                }

            if (compute_capability < 30) shared_size += warp_scan<cur_tpp>::capacity*sizeof(unsigned char)*(block_size/cur_tpp);

            gpu_compute_nlist_binned_shared_kernel<0,cur_tpp><<<grid, block_size,shared_size>>>(d_nlist,
                                                                             d_n_neigh,
                                                                             d_last_updated_pos,
                                                                             d_conditions,
                                                                             d_Nmax,
                                                                             d_head_list,
                                                                             d_pos,
                                                                             d_body,
                                                                             d_diameter,
                                                                             N,
                                                                             d_cell_size,
                                                                             d_cell_xyzf,
                                                                             d_cell_tdb,
                                                                             d_cell_adj,
                                                                             ci,
                                                                             cli,
                                                                             cadji,
                                                                             box,
                                                                             d_r_listsq,
                                                                             ntypes,
                                                                             ghost_width);
            }
        else
            {
            static unsigned int max_block_size = UINT_MAX;
            if (max_block_size == UINT_MAX)
                max_block_size = get_max_block_size(gpu_compute_nlist_binned_shared_kernel<1,cur_tpp>);
            if (compute_capability < 35) gpu_nlist_binned_bind_texture(d_cell_xyzf, cli.getNumElements());

            block_size = block_size < max_block_size ? block_size : max_block_size;
            dim3 grid(N / (block_size/tpp) + 1);
            if (compute_capability < 30 && grid.x > 65535)
                {
                grid.y = grid.x/65535 + 1;
                grid.x = 65535;
                }

            if (compute_capability < 30) shared_size += warp_scan<cur_tpp>::capacity*sizeof(unsigned char)*(block_size/cur_tpp);

            gpu_compute_nlist_binned_shared_kernel<1,cur_tpp><<<grid, block_size,shared_size>>>(d_nlist,
                                                                             d_n_neigh,
                                                                             d_last_updated_pos,
                                                                             d_conditions,
                                                                             d_Nmax,
                                                                             d_head_list,
                                                                             d_pos,
                                                                             d_body,
                                                                             d_diameter,
                                                                             N,
                                                                             d_cell_size,
                                                                             d_cell_xyzf,
                                                                             d_cell_tdb,
                                                                             d_cell_adj,
                                                                             ci,
                                                                             cli,
                                                                             cadji,
                                                                             box,
                                                                             d_r_listsq,
                                                                             ntypes,
                                                                             ghost_width);
            }
        }
    else
        {
        launcher<cur_tpp/2>(d_nlist,
                     d_n_neigh,
                     d_last_updated_pos,
                     d_conditions,
                     d_Nmax,
                     d_head_list,
                     d_pos,
                     d_body,
                     d_diameter,
                     N,
                     d_cell_size,
                     d_cell_xyzf,
                     d_cell_tdb,
                     d_cell_adj,
                     ci,
                     cli,
                     cadji,
                     box,
                     d_r_listsq,
                     ntypes,
                     ghost_width,
                     compute_capability,
                     tpp,
                     filter_body,
                     block_size
                     );
        }
    }

//! template specialization to terminate recursion
template<>
inline void launcher<min_threads_per_particle/2>(unsigned int *d_nlist,
              unsigned int *d_n_neigh,
              Scalar4 *d_last_updated_pos,
              unsigned int *d_conditions,
              const unsigned int *d_Nmax,
              const unsigned int *d_head_list,
              const Scalar4 *d_pos,
              const unsigned int *d_body,
              const Scalar *d_diameter,
              const unsigned int N,
              const unsigned int *d_cell_size,
              const Scalar4 *d_cell_xyzf,
              const Scalar4 *d_cell_tdb,
              const unsigned int *d_cell_adj,
              const Index3D ci,
              const Index2D cli,
              const Index2D cadji,
              const BoxDim box,
              const Scalar *d_r_listsq,
              const unsigned int ntypes,
              const Scalar3 ghost_width,
              const unsigned int compute_capability,
              unsigned int tpp,
              bool filter_body,
              unsigned int block_size)
    { }

cudaError_t gpu_compute_nlist_binned_shared(unsigned int *d_nlist,
                                     unsigned int *d_n_neigh,
                                     Scalar4 *d_last_updated_pos,
                                     unsigned int *d_conditions,
                                     const unsigned int *d_Nmax,
                                     const unsigned int *d_head_list,
                                     const Scalar4 *d_pos,
                                     const unsigned int *d_body,
                                     const Scalar *d_diameter,
                                     const unsigned int N,
                                     const unsigned int *d_cell_size,
                                     const Scalar4 *d_cell_xyzf,
                                     const Scalar4 *d_cell_tdb,
                                     const unsigned int *d_cell_adj,
                                     const Index3D& ci,
                                     const Index2D& cli,
                                     const Index2D& cadji,
                                     const BoxDim& box,
                                     const Scalar *d_r_listsq,
                                     const unsigned int ntypes,
                                     const unsigned int threads_per_particle,
                                     const unsigned int block_size,
                                     bool filter_body,
                                     const Scalar3& ghost_width,
                                     const unsigned int compute_capability)
    {
    launcher<max_threads_per_particle>(d_nlist,
                                   d_n_neigh,
                                   d_last_updated_pos,
                                   d_conditions,
                                   d_Nmax,
                                   d_head_list,
                                   d_pos,
                                   d_body,
                                   d_diameter,
                                   N,
                                   d_cell_size,
                                   d_cell_xyzf,
                                   d_cell_tdb,
                                   d_cell_adj,
                                   ci,
                                   cli,
                                   cadji,
                                   box,
                                   d_r_listsq,
                                   ntypes,
                                   ghost_width,
                                   compute_capability,
                                   threads_per_particle,
                                   filter_body,
                                   block_size
                                   );

    return cudaSuccess;
    }