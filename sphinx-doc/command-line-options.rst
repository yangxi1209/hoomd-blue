.. _command-line-options:

Command line options
====================

Overview
--------

Arguments are processed in :py:func:`hoomd.context.initialize`. Call
:py:func:`hoomd.context.initialize` immediately after importing hoomd so that the requested MPI and GPU options can be
initialized as early as possible.

There are two ways to specify arguments.

 1. On the command line: ``python script.py [options]``::

        import hoomd
        hoomd.context.initialize()

 2. Within your script::

        import hoomd
        hoomd.context.initialize("[options]")

With no arguments, :py:func:`hoomd.context.initialize` will attempt to parse **all** arguments from the command line, whether
it understands them or not. When you pass a string, it ignores the command line (:py:obj:`sys.argv`)
and parses the given string as if it were issued on the command line. In jupyter notebooks, use
``context.initialize("")`` to avoid errors from jupyter specific command line arguments.

Options
-------

* **no options given**

    hoomd will automatically detect the fastest GPU and run on it, or fall back on the CPU if no GPU is found.

* **-h, --help**

    print a description of all the command line options

* **--mode** ={cpu | gpu}

    force hoomd to run either on the cpu or gpu

* **--gpu** =#

    specify the GPU id that hoomd will use. Implies --mode=gpu.

* **--ignore-display-gpu**

    prevent hoomd from using any GPU that is attached to a display

* **--minimize-cpu-usage**

    minimize the CPU usage of hoomd when it runs on a GPU at reduced performance

* **--gpu_error_checking**

    enable error checks after every GPU kernel call

* **--notice-level** =#

    specifies the level of notice messages to print

* **--msg-file=filename**

    specifies a file to write messages (the file is overwritten)

* **--user**

    user options

* *MPI only options*
    * **--nx**

        Number of domains along the x-direction

    * **--ny**

        Number of domains along the y-direction

    * **--nz**

        Number of domains along the z-direction

    * **--linear**

        Force a slab (1D) decomposition along the z-direction

    * **--nrank**

        Number of ranks per partition

    * **--shared-msg-file** =prefix

        specifies the prefix of files to write per-partition output to (filename: *prefix.\<partition_id\>*)

Detailed description
--------------------

Control hoomd execution
^^^^^^^^^^^^^^^^^^^^^^^

HOOMD-blue can run on the CPU or the GPU.  To control which,
set the ``--mode`` option on the script command line. Valid settings are ``cpu``
and ``gpu``::

    python script.py --mode=cpu

When ``--mode`` is set to ``gpu`` and no other options are specified, hoomd will
choose a GPU automatically. It will prioritize the GPU choice based on speed and
whether it is attached to a display. Unless you take steps to configure your system
(see below), then running a second instance of HOOMD-blue will place it on the same GPU
as the first. HOOMD-blue will run correctly with more than one simulation on a GPU as
long as there is enough memory, but at reduced performance.

You can select the GPU on which to run using the ``--gpu`` command line option::

    python script.py --gpu=1

.. note::
    ``--gpu`` implies ``--mode=gpu``. To find out which id
    is assigned to each GPU in your system, download the CUDA SDK for your system
    from http://www.nvidia.com/object/cuda_get.html and run the `deviceQuery` sample.

If you run a script without any options::

    python script.py

hoomd first checks if there are any GPUs in the system. If it finds one or more,
it makes the same automatic choice described previously. If none are found, it runs on the CPU.

Multi-GPU (and multi-CPU) execution
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

HOOMD-blue uses MPI domain decomposition for parallel execution. Execute python with ``mpirun``, ``mpiexec``, or whatever the
appropriate launcher is on your system. For more information, see :ref:`mpi-domain-decomposition`::

    mpirun -n 8 python script.py

All command line options apply to MPI execution in the same way as single process runs.

Automatic free GPU selection
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

You can configure your system for HOOMD-blue to choose free GPUs automatically when each instance is run. To utilize this
capability, the system administrator (root) must first use the ``nvidia-smi`` utility to enable
the compute-exclusive mode on all GPUs in the system. With this mode enabled, running hoomd with no options or with the
``--mode=gpu`` option will result in an automatic choice of the first free GPU from the prioritized list.

The compute-exclusive mode allows *only* a **single CUDA application** to run on each GPU. If you have
4 compute-exclusive GPUs available in the system, executing a fifth instance of hoomd with ``python script.py``
will result in the error: ``***Error! no CUDA-capable device is available``.

Minimize the CPU usage of HOOMD-blue
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When hoomd is running on a GPU, it uses 100% of one CPU core by default. This CPU usage can be
decreased significantly by specifying the ``--minimize-cpu-usage`` command line option::

    python script.py --minimize-cpu-usage

Enabling this option incurs a 10% overall performance reduction, but the CPU usage of hoomd is reduced to only
10% of a single CPU core.

Prevent HOOMD-blue from running on the display GPU
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Running hoomd on the display GPU works just fine, but it does moderately slow the simulation and causes the display
to lag. If you wish to prevent hoomd from running on the display, add the ``--ignore-display-gpu`` command line flag::

    python script.py --ignore-display-gpu

Enable error checking on the GPU
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Detailed error checking is off by default to enable the best performance. If you have trouble
that appears to be caused by the failure of a calculation to run on the GPU, you
should run with GPU error checking enabled to check for any errors returned by the GPU.

To do this, run the script with the ``--gpu_error_checking`` command line option::

    python script.py --gpu_error_checking


Control message output
^^^^^^^^^^^^^^^^^^^^^^

You can adjust the level of messages written to :py:obj:`sys.stdout` by a running hoomd script.
Set the notice level to a high value to help debug where problems occur. Or set it to a low number to suppress messages.
Set it to 0 to remove all notices (warnings and errors are still output)::

    python script.py --notice-level=10

All messages (notices, warnings, and errors) can be redirected to a file. The file is overwritten::

    python script.py --msg-file=messages.out


In MPI simulations, messages can be aggregated per partition. To write output for
partition 0,1,.. in files ``messages.0``, ``messages.1``, etc., use::

    mpirun python script.py --shared-msg-file=messages

Set the MPI domain decomposition
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When no MPI options are specified, HOOMD uses a minimum surface area selection of the domain decomposition strategy::

    mpirun -n 8 python script.py
    # 2x2x2 domain

The linear option forces HOOMD-blue to use a 1D slab domain decomposition, which may be faster than a 3D decomposition when running jobs on a single node::

    mpirun -n 4 python script.py --linear
    # 1x1x4 domain

You can also override the automatic choices completely::

    mpirun -n 4 python script.py --nx=1 --ny=2 --nz=2
    # 1x2x2 domain

You can group multiple MPI ranks into partitions, to simulate independent replicas::

    mpirun -n 12 python script.py --nrank=3

This sub-divides the total of 12 MPI ranks into four independent partitions, with
to which 3 GPUs each are assigned.

User options
^^^^^^^^^^^^

User defined options may be passed to a job script via ``--user`` and retrieved by calling :py:func:`hoomd.option.get_user()`. For example,
if hoomd is executed with::

    python script.py --gpu=2 --ignore-display-gpu --user="--N=5 --rho=0.5"

then :py:func:`hoomd.option.get_user()` will return ``['--N=5', '--rho=0.5']``, which is a format suitable for processing by standard
tools such as :py:obj:`optparse`.
