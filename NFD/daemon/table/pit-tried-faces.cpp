/*
 *  Copyright (c) 2015,  Cisco Systems Inc.
 *  Author: Natalya Rozhnova <natalya.rozhnova@cisco.com>
 */

#include "pit-tried-faces.hpp"

namespace nfd {
TriedFaces::TriedFaces()
{}

void
TriedFaces::ClearAll()
{
	m_triedFaces.clear();
}

bool
TriedFaces::Empty()
{
	if(m_triedFaces.empty())
		return true;
	return false;
}

void
TriedFaces::InsertElement(shared_ptr<Face> face)
{
	if(this->Empty())
	{
		m_triedFaces.push_back(face);
		return;
	}

	TriedFacesList::iterator lit(m_triedFaces.begin());
	for(;lit != m_triedFaces.end(); ++lit)
	{
		shared_ptr<Face> curr_face = (*lit);
		if(curr_face == face)
		{
//			std::cout<<"Face that you are trying to insert is already in the list"<<std::endl;
			return;
		}
	}
//	std::cout<<"Inserting face... "<<face->getId()<<std::endl;
	m_triedFaces.push_back(face);
}

bool
TriedFaces::AlreadyTried(shared_ptr<Face> face)
{
	if(this->Empty())
		return false;
	TriedFacesList::iterator lit(m_triedFaces.begin());
	for(;lit != m_triedFaces.end(); ++lit)
	{
		shared_ptr<nfd::Face> curr_face = (*lit);
/*		std::cout<<"[TriedFaces] face from table "<<curr_face<<" id "<<curr_face->getId()
				 <<" current face "<<face
				 <<" id "<<face->getId()
				 <<std::endl;*/
		if(curr_face == face)
			return true;
	}
	return false;
}

void
TriedFaces::RemoveTried(shared_ptr<Face> face)
{
	if(this->Empty()) return;

	TriedFacesList::iterator lit(m_triedFaces.begin());
	for(;lit != m_triedFaces.end(); ++lit)
	{
		shared_ptr<nfd::Face> curr_face = (*lit);
		if(curr_face == face)
		{
			m_triedFaces.erase(lit);
			return;
		}
	}
}

void
TriedFaces::PrintTriedFaces()
{
	std::cout<<"Tried faces---------------------------------"<<std::endl;
	TriedFacesList::iterator lit(m_triedFaces.begin());
	for(;lit != m_triedFaces.end(); ++lit)
	{
		std::cout<<(*lit)->getId()<<std::endl;
	}
	std::cout<<"That's all----------------------------------"<<std::endl;
}
}
/*
 *  type * x = GetBsrList(...);
 *  if(x){
 *  	type & y = *x;
 *  	type::iterator lit(y.begin())
 *  	etc...
 */
