/*
Highly Optimized Object-Oriented Molecular Dynamics (HOOMD) Open
Source Software License
Copyright (c) 2008 Ames Laboratory Iowa State University
All rights reserved.

Redistribution and use of HOOMD, in source and binary forms, with or
without modification, are permitted, provided that the following
conditions are met:

* Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names HOOMD's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND
CONTRIBUTORS ``AS IS''  AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. 

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS  BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.
*/

// $Id$
// $URL$

/*! \file MSDAnalyzer.h
	\brief Declares the MSDAnalyzer class
*/

#include <string>
#include <fstream>
#include <boost/shared_ptr.hpp>

#include "Analyzer.h"

#ifndef __MSD_ANALYZER_H__
#define __MSD_ANALYZER_H__

//! Describes a group of particles
/*! Some computations in HOOMD may need to only be performed on certain groups of particles. ParticleGroup facilitates 
	that by providing a flexible interface for choosing these groups that can be used by any other class in HOOMD.
	The most common use case is to iterate through all particles in the group, so the class will be optimized for
	that.
	
	The initial implementation only allows selecting particles by type. Future versions will be expanded to allow 
	for more selection criteria.
	
	There are potential issues with the particle type changing over the course of a simulation. Those issues are
	deferred for now. Groups will be evaluated on construction of the group and remain static for its lifetime.
	
	Another issue is how to handle ParticleData? Groups may very well be used inside of a loop where the particle data
	has already been aquired, so ParticleGroup cannot hold onto a shared pointer and aquire again. It can only
	realistically aquire the data on contstruction.
	
	Pulling all these issue together, the best data structure to represent the group is to determine group membership
	on construction and generate a list of particle tags that belong to the group. In this way, iteration through the
	group is efficient and there is no dependance on accessing the ParticleData within the iteration.

	\ingroup data_structs
*/
class ParticleGroup
	{
	public:
		//! Constructs an empty particle group
		ParticleGroup() {};
		
		//! Constructs a particle group of all particles with the given type
		ParticleGroup(boost::shared_ptr<ParticleData> pdata, unsigned int typ);
		
		//! Get the number of members in the group
		/*! \returns The number of particles that belong to this group
		*/
		const unsigned int getNumMembers() const
			{
			return m_members.size();
			}
			
		//! Get a member from the group
		/*! \param i Index from 0 to getNumMembers()-1 of the group member to get
			\returns Tag of the member at index \a i
		*/
		const unsigned int getMemberTag(unsigned int i) const
			{
			assert(i < getNumMembers());
			return m_members[i];
			}
		
	private:
		std::vector<unsigned int> m_members;	//!< Lists the tags of the paritcle members
	};

//! Prints a log of the mean-squared displacement calculated over particles in the simulation
/*! On construction, MSDAnalyzer opens the given file name (overwriting it if it exists) for writing. It also records
	the initial positions of all particles in the simulation. Each time analyze() is called, the mean-squared 
	displacement is calculated and written out to the file.
	
	The mean squared displacement (MSD) is calculated as:
	\f[ \langle |\vec{r} - \vec{r}_0|^2 \rangle \f]
	
	Multiple MSD columns may be desired in a single simulation run. Rather than requiring the user to specify 
	many analyze.msd commands each with a separate file, a single class instance is designed to be capable of outputting
	many columns. The particles over which the MSD is calculated for each column are specified with a ParticleGroup.
	
	\ingroup analyzers
*/
class MSDAnalyzer : public Analyzer
	{
	public:
		//! Construct the msd analyzer
		MSDAnalyzer(boost::shared_ptr<ParticleData> pdata, std::string fname, const std::string& header_prefix="");
		
		//! Write out the data for the current timestep
		void analyze(unsigned int timestep);
		
		//! Sets the delimiter to use between fields
		void setDelimiter(const std::string& delimiter);
		
		//! Adds a column to the analysis
		void addColumn(boost::shared_ptr<ParticleGroup> group, const std::string& name);

	private:
		//! The delimiter to put between columns in the file
		std::string m_delimiter;
		//! The prefix written at the beginning of the header line
		std::string m_header_prefix;	
		
		bool m_columns_changed;	//!< Set to true if the list of columns have changed
		std::ofstream m_file;	//!< The file we write out to
		
		std::vector<Scalar> m_initial_x;	//!< initial value of the x-component listed by tag
		std::vector<Scalar> m_initial_y;	//!< initial value of the y-component listed by tag
		std::vector<Scalar> m_initial_z;	//!< initial value of the z-component listed by tag

		//! struct for storing the particle group and name assocated with a column in the output
		struct column
			{
			//! default constructor
			column() {}
			//! constructs a column
			column(boost::shared_ptr<ParticleGroup> group, const std::string& name) :
				m_group(group), m_name(name) {}
			
			boost::shared_ptr<ParticleGroup> m_group;	//!< A shared pointer to the group definition
			std::string m_name;						//!< The name to print across the file header
			};
		
		std::vector<column> m_columns;	//!< List of groups to output
		
		//! Helper function to write out the header
		void writeHeader();
		//! Helper function to calculate the MSD of a single group
		Scalar calcMSD(boost::shared_ptr<ParticleGroup> group);
		//! Helper function to write one row of output
		void writeRow(unsigned int timestep);
	};	
	
//! Exports the MSDAnalyzer class to python
void export_MSDAnalyzer();
//! Exports the ParticleGroup class to python
void export_ParticleGroup();

#endif