// Copyright (c) 2009-2017 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.

#include "OBBTree.h"

#ifndef __GPU_TREE_H__
#define __GPU_TREE_H__

// need to declare these class methods with __device__ qualifiers when building in nvcc
// DEVICE is __host__ __device__ when included in nvcc and blank when included into the host compiler
#ifdef NVCC
#define DEVICE __device__
#else
#define DEVICE
#endif

#ifndef NVCC
#include <sstream>
#endif

namespace hpmc
{

namespace detail
{

//! Adapter class to AABTree for query on the GPU
template<unsigned int max_nodes, unsigned int node_capacity>
class GPUTree
    {
    public:
        #ifndef NVCC
        typedef OBBTree<node_capacity> obb_tree_type;
        #endif

        enum { capacity = node_capacity } Enum;

        //! Empty constructor
        GPUTree()
            : m_num_nodes(0)
            { }

        #ifndef NVCC
        //! Constructor
        /*! \param tree OBBTree to construct from
         */
        GPUTree(const obb_tree_type &tree)
            {
            if (tree.getNumNodes() >= max_nodes)
                {
                std::ostringstream oss;
                oss << "GPUTree: Too many nodes (" << tree.getNumNodes() << " > " << max_nodes << ")" << std::endl;
                throw std::runtime_error(oss.str());
                }

            // load data from AABTree
            for (unsigned int i = 0; i < tree.getNumNodes(); ++i)
                {
                m_left[i] = tree.getNodeLeft(i);
                m_skip[i] = tree.getNodeSkip(i);

                m_center[i] = tree.getNodeOBB(i).getPosition();
                m_rotation[i] = tree.getNodeOBB(i).rotation;
                m_lengths[i] = tree.getNodeOBB(i).lengths;

               for (unsigned int j = 0; j < capacity; ++j)
                    {
                    if (j < tree.getNodeNumParticles(i))
                        {
                        m_particles[i*capacity+j] = tree.getNodeParticle(i,j);
                        }
                    else
                        {
                        m_particles[i*capacity+j] = -1;
                        }
                    }
                }
            m_num_nodes = tree.getNumNodes();

            // update auxillary information for tandem traversal
            updateRCL(0, tree, 0, true, m_num_nodes, 0);
            }
        #endif

        //! Returns number of nodes in tree
        DEVICE unsigned int getNumNodes() const
            {
            return m_num_nodes;
            }

        #if 0
        //! Fetch the next node in the tree and test against overlap
        /*! The method maintains it internal state in a user-supplied variable cur_node
         *
         * \param obb Query bounding box
         * \param cur_node If 0, start a new tree traversal, otherwise use stored value from previous call
         * \param particles List of particles returned (array of at least capacity length), -1 means no particle
         * \returns true if the current node overlaps and is a leaf node
         */
        DEVICE inline bool queryNode(const OBB& obb, unsigned int &cur_node, int *particles) const
            {
            OBB node_obb(m_lower[cur_node],m_upper[cur_node]);

            bool leaf = false;
            if (overlap(node_obb, obb))
                {
                // is this node a leaf node?
                if (m_left[cur_node] == INVALID_NODE)
                    {
                    for (unsigned int i = 0; i < capacity; i++)
                        particles[i] = m_particles[cur_node*capacity+i];
                    leaf = true;
                    }
                }
            else
                {
                // skip ahead
                cur_node += m_skip[cur_node];
                }

            // advance cur_node
            cur_node ++;

            return leaf;
            }
        #endif

        //! Test if a given index is a leaf node
        DEVICE inline bool isLeaf(unsigned int idx) const
            {
            return (m_left[idx] == OBB_INVALID_NODE);
            }

        DEVICE inline int getParticle(unsigned int node, unsigned int i) const
            {
            return m_particles[node*capacity+i];
            }

        DEVICE inline unsigned int getLevel(unsigned int node) const
            {
            return m_level[node];
            }

        DEVICE inline unsigned int getLeftChild(unsigned int node) const
            {
            return m_left[node];
            }

        DEVICE inline bool isLeftChild(unsigned int node) const
            {
            return m_isleft[node];
            }

        DEVICE inline unsigned int getParent(unsigned int node) const
            {
            return m_parent[node];
            }

        DEVICE inline unsigned int getRCL(unsigned int node) const
            {
            return m_rcl[node];
            }

        DEVICE inline void advanceNode(unsigned int &cur_node, bool skip) const
            {
            if (skip) cur_node += m_skip[cur_node];
            cur_node++;
            }

        DEVICE inline OBB getOBB(unsigned int idx) const
            {
            OBB obb;
            obb.center = m_center[idx];
            obb.lengths = m_lengths[idx];
            obb.rotation = m_rotation[idx];
            return obb;
            }

    protected:
        #ifndef NVCC
        void updateRCL(unsigned int idx, const obb_tree_type& tree, unsigned int level, bool left,
             unsigned int parent_idx, unsigned int rcl)
            {
            if (!isLeaf(idx))
                {
                unsigned int left_idx = tree.getNodeLeft(idx);;
                unsigned int right_idx = tree.getNode(idx).right;

                updateRCL(left_idx, tree, level+1, true, idx, 0);
                updateRCL(right_idx, tree, level+1, false, idx, rcl+1);
                }
            m_level[idx] = level;
            m_isleft[idx] = left;
            m_parent[idx] = parent_idx;
            m_rcl[idx] = rcl;
            }
        #endif

    private:
        vec3<OverlapReal> m_center[max_nodes];
        vec3<OverlapReal> m_lengths[max_nodes];
        rotmat3<OverlapReal> m_rotation[max_nodes];

        unsigned int m_level[max_nodes];              //!< Depth
        bool m_isleft[max_nodes];                     //!< True if this node is a left node
        unsigned int m_parent[max_nodes];             //!< Pointer to parent
        unsigned int m_rcl[max_nodes];                //!< Right child level

        int m_particles[max_nodes*node_capacity];     //!< Stores the nodes' indices

        unsigned int m_left[max_nodes];               //!< Left nodes
        unsigned int m_skip[max_nodes];               //!< Skip intervals
        unsigned int m_num_nodes;                     //!< Number of nodes in the tree
    };

//! Test a subtree against a leaf node during a tandem traversal
template<class Shape, class Tree>
DEVICE inline bool test_subtree(const vec3<OverlapReal>& r_ab,
                                const Shape& s0,
                                const Shape& s1,
                                const Tree& tree_a,
                                const Tree& tree_b,
                                unsigned int leaf_node,
                                unsigned int cur_node,
                                unsigned int end_idx)
    {
    // get the obb of the leaf node
    hpmc::detail::OBB obb = tree_a.getOBB(leaf_node);
    obb.affineTransform(conj(quat<OverlapReal>(s1.orientation))*quat<OverlapReal>(s0.orientation),
        rotate(conj(quat<OverlapReal>(s1.orientation)),-r_ab));

    while (cur_node != end_idx)
        {
        hpmc::detail::OBB node_obb = tree_b.getOBB(cur_node);

        bool skip = false;

        if (detail::overlap(obb,node_obb))
            {
            if (tree_b.isLeaf(cur_node))
                {
                if (test_narrow_phase_overlap(r_ab, s0, s1, leaf_node, cur_node)) return true;
                }
            }
        else
            {
            skip = true;
            }
        tree_b.advanceNode(cur_node, skip);
        }
    return false;
    }

//! Move up during a tandem traversal, alternating between trees a and b
/*! Adapted from: "Stackless BVH Collision Detection for Physical Simulation" by
    Jesper Damkjaer, damkjaer@diku.edu, http://image.diku.dk/projects/media/jesper.damkjaer.07.pdf
 */
template<class Tree>
DEVICE inline void moveUp(const Tree& tree_a, unsigned int& cur_node_a, const Tree& tree_b, unsigned int& cur_node_b)
    {
    unsigned int level_a = tree_a.getLevel(cur_node_a);
    unsigned int level_b = tree_b.getLevel(cur_node_b);

    if (level_a == level_b)
        {
        bool a_is_left_child = tree_a.isLeftChild(cur_node_a);
        bool b_is_left_child = tree_b.isLeftChild(cur_node_b);
        if (a_is_left_child)
            {
            tree_a.advanceNode(cur_node_a, true);
            return;
            }
        if (!a_is_left_child && b_is_left_child)
            {
            cur_node_a = tree_a.getParent(cur_node_a);
            tree_b.advanceNode(cur_node_b, true);
            return;
            }
        if (!a_is_left_child && !b_is_left_child)
            {
            unsigned int rcl_a = tree_a.getRCL(cur_node_a);
            unsigned int rcl_b = tree_b.getRCL(cur_node_b);
            if (rcl_a <= rcl_b)
                {
                tree_a.advanceNode(cur_node_a, true);
                // LevelUp
                while (rcl_a)
                    {
                    cur_node_b = tree_b.getParent(cur_node_b);
                    rcl_a--;
                    }
                }
            else
                {
                // LevelUp
                rcl_b++;
                while (rcl_b)
                    {
                    cur_node_a = tree_a.getParent(cur_node_a);
                    rcl_b--;
                    }
                tree_b.advanceNode(cur_node_b, true);
                }
            return;
            }
        } // end if level_a == level_b
    else
        {
        bool a_is_left_child = tree_a.isLeftChild(cur_node_a);
        bool b_is_left_child = tree_b.isLeftChild(cur_node_b);

        if (b_is_left_child)
            {
            tree_b.advanceNode(cur_node_b, true);
            return;
            }
        if (a_is_left_child)
            {
            tree_a.advanceNode(cur_node_a, true);
            cur_node_b = tree_b.getParent(cur_node_b);
            return;
            }
        if (!a_is_left_child && !b_is_left_child)
            {
            unsigned int rcl_a = tree_a.getRCL(cur_node_a);
            unsigned int rcl_b = tree_b.getRCL(cur_node_b);

            if (rcl_a <= rcl_b-1)
                {
                tree_a.advanceNode(cur_node_a, true);
                // LevelUp
                rcl_a++;
                while (rcl_a)
                    {
                    cur_node_b = tree_b.getParent(cur_node_b);
                    rcl_a--;
                    }
                }
            else
                {
                // LevelUp
                while (rcl_b)
                    {
                    cur_node_a = tree_a.getParent(cur_node_a);
                    rcl_b--;
                    }
                tree_b.advanceNode(cur_node_b, true);
                }
            return;
            }
        }
    }

}; // end namespace detail

}; // end namespace hpmc

#endif // __GPU_TREE_H__
