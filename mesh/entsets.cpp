// Copyright (c) 2010, Lawrence Livermore National Security, LLC. Produced at
// the Lawrence Livermore National Laboratory. LLNL-CODE-443211. All Rights
// reserved. See file COPYRIGHT for details.
//
// This file is part of the MFEM library. For more information and source code
// availability see http://mfem.org.
//
// MFEM is free software; you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License (as published by the Free
// Software Foundation) version 2.1 dated February 1999.


#include "entsets.hpp"
#include "mesh.hpp"

using namespace std;

namespace mfem
{

EntitySets::EntitySets(Mesh & mesh)
   : mesh_(&mesh),
     edge_vertex_(NULL),
     face_vertex_(NULL),
     face_edge_(NULL),
     NumOfVertices_(mesh.GetNV()),
     NumOfEdges_(mesh.GetNEdges()),
     NumOfElements_(mesh.GetNE()),
     sets_(4),
     set_names_(4),
     set_index_by_name_(4)
{
}

EntitySets::EntitySets(const EntitySets & ent_sets)
   : mesh_(ent_sets.GetMesh()),
     edge_vertex_(NULL),
     face_vertex_(NULL),
     face_edge_(NULL),
     NumOfVertices_(mesh_->GetNV()),
     NumOfEdges_(mesh_->GetNEdges()),
     NumOfElements_(mesh_->GetNE()),
     sets_(4),
     set_names_(4),
     set_index_by_name_(4)
{
   this->CopyEntitySets(ent_sets, VERTEX);
   this->CopyEntitySets(ent_sets, EDGE);
   this->CopyEntitySets(ent_sets, FACE);
   this->CopyEntitySets(ent_sets, ELEMENT);

   if ( ent_sets.GetEdgeVertexTable() )
   {
      edge_vertex_ = new Table(*ent_sets.GetEdgeVertexTable());
   }
   if ( ent_sets.GetFaceVertexTable() )
   {
      face_vertex_ = new Table(*ent_sets.GetFaceVertexTable());
   }
   if ( ent_sets.GetFaceEdgeTable() )
   {
      face_edge_ = new Table(*ent_sets.GetFaceEdgeTable());
   }
}

EntitySets::EntitySets(Mesh & mesh, NCMesh &ncmesh)
   : mesh_(&mesh),
     edge_vertex_(NULL),
     face_vertex_(NULL),
     face_edge_(NULL),
     NumOfVertices_(mesh_->GetNV()),
     NumOfEdges_(mesh_->GetNEdges()),
     NumOfElements_(mesh_->GetNE()),
     sets_(4),
     set_names_(4),
     set_index_by_name_(4)
{
   this->BuildEntitySets(ncmesh, VERTEX);
   this->BuildEntitySets(ncmesh, EDGE);
   this->BuildEntitySets(ncmesh, FACE);
   this->BuildEntitySets(ncmesh, ELEMENT);
}

EntitySets::~EntitySets()
{
   delete edge_vertex_;
   delete face_edge_;
   delete face_vertex_;
}

void
EntitySets::Load(istream &input)
{
   if (!input)
   {
      MFEM_ABORT("Input stream is not open");
   }

   string file_type;
   input >> ws;
   getline(input, file_type);
   filter_dos(file_type);

   bool mfem_sets_v10 = (file_type == "MFEM sets v1.0");
   if ( mfem_sets_v10 )
   {
      string ident;
      int Dim = -1;

      // read lines beginning with '#' (comments)
      skip_comment_lines(input, '#');
      input >> ident >> ws; // 'dimension'

      MFEM_VERIFY(ident == "dimension", "invalid entity set file");
      input >> Dim >> ws;

      LoadEntitySets(input, VERTEX, "vertex_sets");

      if ( Dim > 1 )
      {
         LoadEntitySets(input, EDGE, "edge_sets");
      }
      if ( Dim > 2 )
      {
         LoadEntitySets(input, FACE, "face_sets");
      }

      LoadEntitySets(input, ELEMENT, "element_sets");
   }
   else
   {
      // Unknown input entity set format so this file has no entity sets
      return;
   }
   CopyMeshTables();
}

void
EntitySets::LoadEntitySets(istream &input, EntityType t, const string & header)
{
   string ident;
   int NumSets = -1;
   int NumEntities = -1;
   int g, v0, v1, v2, v3;

   DSTable  *v_to_v   = NULL;
   STable3D *face_tbl = NULL;

   skip_comment_lines(input, '#');
   input >> ident >> ws;

   MFEM_VERIFY(ident == header, "invalid entity set file");
   input >> NumSets >> ws;

   sets_[t].resize(NumSets);
   set_names_[t].resize(NumSets);

   if ( NumSets > 0 )
   {
      if ( t == EDGE )
      {
         v_to_v = new DSTable(NumOfVertices_);
         mesh_->GetVertexToVertexTable(*v_to_v);
      }
      else if ( t == FACE )
      {
         face_tbl = mesh_->GetFacesTable();
      }
   }

   for (int i=0; i<NumSets; i++)
   {
      getline(input, ident);
      filter_dos(ident);

      set_names_[t][i] = ident;
      set_index_by_name_[t][ident] = i;

      input >> NumEntities;
      sets_[t][i].resize(NumEntities);

      for (int j=0; j<NumEntities; j++)
      {
         switch (t)
         {
            case VERTEX:
            case ELEMENT:
               // Read vertex or element index
               input >> sets_[t][i][j];
               break;
            case EDGE:
               // Read two segment vertices
               input >> v0 >> v1;
               sets_[t][i][j] = (*v_to_v)(v0,v1);
               break;
            case FACE:
               // Read geometry type
               input >> g;
               if ( g == 2 )
               {
                  // Read triangle vertices
                  input >> v0 >> v1 >> v2;
                  sets_[t][i][j] = (*face_tbl)(v0, v1, v2);
               }
               else if ( g == 3)
               {
                  // Read quadrilateral vertices
                  input >> v0 >> v1 >> v2 >> v3;
                  sets_[t][i][j] = (*face_tbl)(v0, v1, v2, v3);
               }
               else
               {
                  MFEM_ABORT("Unknown face geometry type format: \""
                             << g << "\"");
               }
               break;
            default:
               MFEM_ABORT("Unknown entity set format: \"" << t << "\"");
         }
         input >> ws;
      }
   }
   delete v_to_v;
   delete face_tbl;
}

void
EntitySets::Print(std::ostream &output) const
{
   output << "MFEM sets v1.0" << endl << endl;
   output << "dimension" << endl << mesh_->Dimension() << endl << endl;

   this->PrintEntitySets(output, VERTEX, "vertex_sets");

   if ( mesh_->Dimension() > 1 )
   {
      this->PrintEdgeSets(output);
   }

   if ( mesh_->Dimension() > 2 )
   {
      this->PrintFaceSets(output);
   }

   this->PrintEntitySets(output, ELEMENT, "element_sets");
}

void
EntitySets::PrintEntitySets(std::ostream &output, EntityType t,
                            const std::string & header) const
{
   output << header << endl << sets_[t].size() << endl << endl;
   for (unsigned int s=0; s<sets_[t].size(); s++)
   {
      output << set_names_[t][s] << endl << sets_[t][s].size() << endl;
      for (unsigned int i=0; i<sets_[t][s].size(); i++)
      {
         output << sets_[t][s][i];
         if ( i < sets_[t][s].size() - 1 )
         {
            output << " ";
         }
         else
         {
            output << endl << endl;
         }
      }
   }
}

void
EntitySets::PrintEdgeSets(std::ostream &output) const
{
   EntityType t = EDGE;

   output << "edge_sets" << endl << sets_[t].size() << endl << endl;
   for (unsigned int s=0; s<sets_[t].size(); s++)
   {
      output << set_names_[t][s] << endl << sets_[t][s].size() << endl;
      for (unsigned int i=0; i<sets_[t][s].size(); i++)
      {
         int edge = sets_[t][s][i];
         if ( edge < 0 )
         {
            output << "bad_edge";
            if ( i < sets_[t][s].size() - 1 )
            {
               output << " ";
            }
            else
            {
               output << endl << endl;
            }
            continue;
         }
         int *v = edge_vertex_->GetRow(edge);
         output << v[0] << " " << v[1];
         if ( i < sets_[t][s].size() - 1 )
         {
            output << " ";
         }
         else
         {
            output << endl << endl;
         }
      }
   }
}

void
EntitySets::PrintFaceSets(std::ostream &output) const
{
   EntityType t = FACE;

   output << "face_sets" << endl << sets_[t].size() << endl << endl;
   for (unsigned int s=0; s<sets_[t].size(); s++)
   {
      output << set_names_[t][s] << endl << sets_[t][s].size() << endl;
      for (unsigned int i=0; i<sets_[t][s].size(); i++)
      {
         int face = sets_[t][s][i];
         if ( face < 0 )
         {
            output << "bad_face";
            if ( i < sets_[t][s].size() - 1 )
            {
               output << " ";
            }
            else
            {
               output << endl << endl;
            }
            continue;
         }
         int numv = face_vertex_->RowSize(face);
         int *v = face_vertex_->GetRow(face);
         output << numv - 1;
         for (int j=0; j<numv; j++)
         {
            output << " " << v[j];
         }
         output << endl;
      }
      output << endl;
   }
}

void
EntitySets::PrintSetInfo(std::ostream & output) const
{
   if ( GetNumSets(VERTEX) > 0 || GetNumSets(EDGE)    > 0 ||
        GetNumSets(FACE)   > 0 || GetNumSets(ELEMENT) > 0 )
   {
      output << "\nMFEM Entity Sets:\n";
   }
   this->PrintEntitySetInfo(output, VERTEX,  "Vertex");
   this->PrintEntitySetInfo(output, EDGE,    "Edge");
   this->PrintEntitySetInfo(output, FACE,    "Face");
   this->PrintEntitySetInfo(output, ELEMENT, "Element");
}

void
EntitySets::PrintEntitySetInfo(std::ostream & output, EntityType t,
                               const string & ent_name) const
{
   if ( sets_[t].size() > 0 )
   {
      output << "  " << ent_name << " Sets (Index, Size, Set Name):\n";
      for (unsigned int s=0; s<sets_[t].size(); s++)
      {
         output << '\t' << s
                << '\t' << sets_[t][s].size()
                << '\t' << set_names_[t][s]
                << '\n';
      }
      output << '\n';
   }
}

void
EntitySets::CopyEntitySets(const EntitySets & ent_sets, EntityType t)
{
   unsigned int ns = ent_sets.GetNumSets(t);
   sets_[t].resize(ns);
   set_names_[t].resize(ns);
   for (unsigned int s=0; s<ns; s++)
   {
      int ni = ent_sets.GetNumEntities(t, s);
      sets_[t][s].resize(ni);
      set_names_[t][s] = ent_sets.GetSetName(t, s);
      set_index_by_name_[t][set_names_[t][s]] = s;

      for (int i=0; i<ni; i++)
      {
         sets_[t][s][i] = ent_sets(t, s, i);
      }
   }
}

void
EntitySets::BuildEntitySets(NCMesh &ncmesh, EntityType t)
{
   int es = ncmesh.ncent_sets->GetEntitySize(t);
   unsigned int ns = ncmesh.ncent_sets->GetNumSets(t);

   Array<int> inds(es);

   sets_[t].resize(ns);
   set_names_[t].resize(ns);
   for (unsigned int s=0; s<ns; s++)
   {
      int ni = ncmesh.ncent_sets->GetNumEntities(t, s);
      set_names_[t][s] = ncmesh.ncent_sets->GetSetName(t, s);
      set_index_by_name_[t][set_names_[t][s]] = s;

      BlockArray<int> set;

      switch (t)
      {
         case VERTEX:
            sets_[t][s].resize(ni);
            for (int i=0; i<ni; i++)
            {
 	       sets_[t][s][i] = (*ncmesh.ncent_sets)(t, s, i);
            }
            break;
         case EDGE:
            for (int i=0; i<ni; i++)
            {
               ncmesh.ncent_sets->GetEntityIndex(t, s, i, inds);
               BlockArray<int> ind_coll;
               ncmesh.GetRefinedEdges(inds[0], inds[1],
                                      ind_coll);

               for (int j=0; j<ind_coll.Size(); j++)
               {
                  set.Append(ind_coll[j]);
               }
            }
            break;
         case FACE:
            for (int i=0; i<ni; i++)
            {
               ncmesh.ncent_sets->GetEntityIndex(t, s, i, inds);
               BlockArray<int> ind_coll;
               ncmesh.GetRefinedFaces(inds[0], inds[1], inds[2], inds[3],
                                      ind_coll);

               for (int j=0; j<ind_coll.Size(); j++)
               {
                  set.Append(ind_coll[j]);
               }
            }
            break;
         case ELEMENT:
            for (int i=0; i<ni; i++)
            {
	       int elem = (*ncmesh.ncent_sets)(t, s, i);
               BlockArray<int> ind_coll;
               ncmesh.GetRefinedElements(elem, ind_coll);

               for (int j=0; j<ind_coll.Size(); j++)
               {
                  set.Append(ind_coll[j]);
               }
            }
            break;
         default:
            MFEM_ABORT("Unknown entity set format: \"" << t << "\"");
      }

      if ( t != VERTEX )
      {
         sets_[t][s].resize(set.Size());
         for (int j=0; j<set.Size(); j++)
         {
            sets_[t][s][j] = set[j];
         }
      }
   }
}

unsigned int
EntitySets::GetNumSets(EntityType t) const
{
   MFEM_ASSERT( t >= VERTEX && t <= ELEMENT,
                "EntitySets Invalid entity type \"" << t << "\"");
   return sets_[t].size();
}

const string &
EntitySets::GetSetName(EntityType t, int s) const
{
   MFEM_ASSERT( t >= VERTEX && t <= ELEMENT,
                "EntitySets Invalid entity type \"" << t << "\"");
   return set_names_[t][s];
}

int
EntitySets::GetSetIndex(EntityType t, const string & s) const
{
   MFEM_ASSERT( t >= VERTEX && t <= ELEMENT,
                "EntitySets Invalid entity type \"" << t << "\"");

   map<string,int>::const_iterator mit = set_index_by_name_[t].find(s);

   MFEM_ASSERT( mit != set_index_by_name_[t].end(),
                "EntitySets unrecognized set name \"" << s
                << "\" for entity type \"" << t << "\"" );

   return mit->second;
}

unsigned int
EntitySets::GetNumEntities(EntityType t, int s) const
{
   MFEM_ASSERT( t >= VERTEX && t <= ELEMENT,
                "EntitySets Invalid entity type \"" << t << "\"");
   return sets_[t][s].size();
}

unsigned int
EntitySets::GetNumEntities(EntityType t, const std::string & s) const
{
   return this->GetNumEntities(t, this->GetSetIndex(t, s));
}

int
EntitySets::GetEntityIndex(EntityType t, int s, int i) const
{
   MFEM_ASSERT( t >= VERTEX && t <= ELEMENT,
                "EntitySets Invalid entity type \"" << t << "\"");
   return sets_[t][s][i];
}

int
EntitySets::GetEntityIndex(EntityType t, const string & s, int i) const
{
   return this->GetEntityIndex(t, this->GetSetIndex(t, s), i);
}

void
EntitySets::CopyMeshTables()
{
   if ( this->GetNumSets(EDGE) > 0 )
   {
      if ( edge_vertex_ ) { delete edge_vertex_; }
      if ( mesh_->edge_vertex )
      {
         edge_vertex_ = new Table(*mesh_->edge_vertex);
      }
      else
      {
         edge_vertex_ = mesh_->GetEdgeVertexTable();
         mesh_->edge_vertex = NULL;
      }
   }
   if ( this->GetNumSets(FACE) > 0 )
   {
      if ( face_vertex_ ) { delete face_vertex_; }
      if ( mesh_->face_vertex )
      {
         face_vertex_ = new Table(*mesh_->face_vertex);
      }
      else
      {
         face_vertex_ = mesh_->GetFaceVertexTable();
         mesh_->face_vertex = NULL;
      }
   }
   if ( this->GetNumSets(FACE) > 0 )
   {
      if ( face_edge_ ) { delete face_edge_; }
      if ( mesh_->face_edge )
      {
         face_edge_ = new Table(*mesh_->face_edge);
      }
      else
      {
         face_edge_ = mesh_->GetFaceEdgeTable();
         mesh_->face_edge = NULL;
      }
   }

   NumOfVertices_ = mesh_->GetNV();
   NumOfEdges_    = mesh_->GetNEdges();
   NumOfElements_ = mesh_->GetNE();
}

void
EntitySets::QuadUniformRefinement()
{
   // VERTEX Sets will be unchanged

   // EDGE Sets will need to be augmented
   if ( this->GetNumSets(EDGE) > 0 )
   {
      EntityType t = EDGE;
      DSTable * v_to_v = new DSTable(mesh_->GetNV());
      mesh_->GetVertexToVertexTable(*v_to_v);

      int oedge = NumOfVertices_;
      for (unsigned int s=0; s<this->GetNumSets(t); s++)
      {
         int n = this->GetNumEntities(t, s);
         (*this)(t, s).resize(2 * n);
         for (int i=0; i<n; i++)
         {
            int old_edge = (*this)(t, s, i);
            int *v = edge_vertex_->GetRow(old_edge);
            (*this)(t, s, i)     = (*v_to_v)(v[0], oedge + old_edge);
            (*this)(t, s, i + n) = (*v_to_v)(v[1], oedge + old_edge);
         }
      }
      delete v_to_v;
   }

   // FACE Sets do not exist in 2D

   // ELEMENT Sets will need to be augmented
   EntityType t = ELEMENT;
   for (unsigned int s=0; s<this->GetNumSets(t); s++)
   {
      int n = this->GetNumEntities(t, s);
      (*this)(t, s).resize(4 * n);
      for (int i=0; i<n; i++)
      {
         int e = (*this)(t, s)[i];
         int j = NumOfElements_ + 3 * e;
         (*this)(t, s, n + 3 * i + 0) = j + 0;
         (*this)(t, s, n + 3 * i + 1) = j + 1;
         (*this)(t, s, n + 3 * i + 2) = j + 2;
      }
   }

   this->CopyMeshTables();
}

void
EntitySets::HexUniformRefinement()
{
   // VERTEX Sets will be unchanged

   // EDGE Sets will need to be augmented
   DSTable * v_to_v = NULL;
   if ( this->GetNumSets(EDGE) > 0 ||
        this->GetNumSets(FACE) > 0 )
   {
      v_to_v = new DSTable(mesh_->GetNV());
      mesh_->GetVertexToVertexTable(*v_to_v);
   }
   if ( this->GetNumSets(EDGE) > 0 )
   {
      EntityType t = EDGE;
      int oedge = NumOfVertices_;
      for (unsigned int s=0; s<this->GetNumSets(t); s++)
      {
         int n = this->GetNumEntities(t, s);
         (*this)(t, s).resize(2 * n);
         for (int i=0; i<n; i++)
         {
            int old_edge = (*this)(t, s, i);
            int *v = edge_vertex_->GetRow(old_edge);
            (*this)(t, s, i)     = (*v_to_v)(v[0], oedge + old_edge);
            (*this)(t, s, i + n) = (*v_to_v)(v[1], oedge + old_edge);
         }
      }
   }
   delete v_to_v;

   // FACE Sets will need to be augmented
   if ( this->GetNumSets(FACE) > 0 )
   {
      EntityType t = FACE;
      STable3D * faces_tbl = mesh_->GetFacesTable();

      int oedge = NumOfVertices_;
      int oface = oedge + NumOfEdges_;

      for (unsigned int s=0; s<this->GetNumSets(t); s++)
      {
         int n = this->GetNumEntities(t, s);
         (*this)(t, s).resize(4 * n);
         for (int i=0; i<n; i++)
         {
            int old_face = (*this)(t, s, i);
            int * v = face_vertex_->GetRow(old_face);
            int * e = face_edge_->GetRow(old_face);

            for (int j=0; j<4; j++)
            {
               int v0 = v[j];
               int v3 = oface + old_face;
               for (int k=0; k<4; k++)
               {
                  int v1 = oedge + e[k];
                  int new_face = -1;
                  for (int l=1; l<4; l++)
                  {
                     int v2 = oedge + e[(k+l)%4];
                     new_face = (*faces_tbl)(v0,v1,v2,v3);
                     if ( new_face >= 0 )
                     {
                        if ( j == 0 )
                        {
                           (*this)(t,s,i) = new_face;
                        }
                        else
                        {
                           (*this)(t,s,n+3*i+j-1) = new_face;
                        }
                        break;
                     }
                  }
                  if ( new_face >= 0 )
                  {
                     break;
                  }
               }
            }
         }
      }
      delete faces_tbl;
   }

   // ELEMENT Sets will need to be augmented
   EntityType t = ELEMENT;
   for (unsigned int s=0; s<this->GetNumSets(t); s++)
   {
      int n = this->GetNumEntities(t, s);
      (*this)(t, s).resize(8 * n);
      for (int i=0; i<n; i++)
      {
         int e = (*this)(t, s)[i];
         int j = NumOfElements_ + 7 * e;
         (*this)(t, s, n + 7 * i + 0) = j + 0;
         (*this)(t, s, n + 7 * i + 1) = j + 1;
         (*this)(t, s, n + 7 * i + 2) = j + 2;
         (*this)(t, s, n + 7 * i + 3) = j + 3;
         (*this)(t, s, n + 7 * i + 4) = j + 4;
         (*this)(t, s, n + 7 * i + 5) = j + 5;
         (*this)(t, s, n + 7 * i + 6) = j + 6;
      }
   }

   this->CopyMeshTables();
}

NCEntitySets::NCEntitySets(const Mesh &mesh, const NCMesh &ncmesh)
   : ncmesh_(mesh.ncmesh),
     sets_(4),
     set_names_(4),
     set_index_by_name_(4),
     entity_size_(4)
{
   cout << "Entering NCEntitySets(Mesh) c'tor" << endl;

   if ( mesh.ent_sets )
   {
      EntitySets::EntityType t = EntitySets::INVALID;
      unsigned int ns = -1;

      std::cout << "Processing node sets" << std::endl;

      t = EntitySets::VERTEX;
      entity_size_[t] = 1;
      ns = mesh.ent_sets->GetNumSets(t);

      sets_[t].resize(ns);
      set_names_[t].resize(ns);

      for (unsigned int s=0; s<ns; s++)
      {
         unsigned int ni = mesh.ent_sets->GetNumEntities(t, s);

         sets_[t][s].resize(ni);
         set_names_[t][s] = mesh.ent_sets->GetSetName(t, s);
         set_index_by_name_[t][set_names_[t][s]] = s;

         for (unsigned int i=0; i<ni; i++)
         {
            sets_[t][s][i] = mesh.ent_sets->GetEntityIndex(t, s, i);
         }
      }

      std::cout << "Processing edge sets" << std::endl;
      if (mesh.ent_sets->edge_vertex_ == NULL)
      { std::cout << "edge_vertex table is NULL" << std::endl;}
      t = EntitySets::EDGE;
      entity_size_[t] = 2;
      ns = mesh.ent_sets->GetNumSets(t);

      sets_[t].resize(ns);
      set_names_[t].resize(ns);

      for (unsigned int s=0; s<ns; s++)
      {
         unsigned int ni = mesh.ent_sets->GetNumEntities(t, s);

         sets_[t][s].resize(2 * ni);
         set_names_[t][s] = mesh.ent_sets->GetSetName(t, s);
         set_index_by_name_[t][set_names_[t][s]] = s;

         for (unsigned int i=0; i<ni; i++)
         {
            int edge = mesh.ent_sets->GetEntityIndex(t, s, i);
            int *v = mesh.ent_sets->edge_vertex_->GetRow(edge);
            sets_[t][s][2 * i + 0] = v[0];
            sets_[t][s][2 * i + 1] = v[1];
         }
      }

      std::cout << "Processing face sets" << std::endl;

      t = EntitySets::FACE;
      entity_size_[t] = 4;
      ns = mesh.ent_sets->GetNumSets(t);

      sets_[t].resize(ns);
      set_names_[t].resize(ns);

      for (unsigned int s=0; s<ns; s++)
      {
         unsigned int ni = mesh.ent_sets->GetNumEntities(t, s);

         sets_[t][s].resize(4 * ni);
         set_names_[t][s] = mesh.ent_sets->GetSetName(t, s);
         set_index_by_name_[t][set_names_[t][s]] = s;

         for (unsigned int i=0; i<ni; i++)
         {
            int face = mesh.ent_sets->GetEntityIndex(t, s, i);
            int *v = mesh.ent_sets->face_vertex_->GetRow(face);
            int nv = mesh.ent_sets->face_vertex_->RowSize(face);
            sets_[t][s][4 * i + 0] = v[0];
            sets_[t][s][4 * i + 1] = v[1];
            sets_[t][s][4 * i + 2] = v[2];
            if ( nv > 3 )
            {
               sets_[t][s][4 * i + 3] = v[3];

               // Unfortunately the face_vertex table doesn not store
               // the topological order of the face vertices but we
               // need this to find refined faces.  To recover this
               // information we verify that edges connect vertex 0 to
               // vertices 1 and 3.  If not then we swap either 1 or 3
               // with vertex 2.
               int e01 = ncmesh.nodes.FindId(v[0], v[1]);
               int e03 = ncmesh.nodes.FindId(v[0], v[3]);

               if ( e01 < 0 )
               {
                  Swap(sets_[t][s][4 * i + 1],sets_[t][s][4 * i + 2]);
               }
               else if ( e03 < 0 )
               {
                  Swap(sets_[t][s][4 * i + 3],sets_[t][s][4 * i + 2]);
               }
            }
            else
            {
               sets_[t][s][4 * i + 3] = -1;
            }

         }
      }

      std::cout << "Processing element sets" << std::endl;

      t = EntitySets::ELEMENT;
      entity_size_[t] = 1;
      ns = mesh.ent_sets->GetNumSets(t);

      sets_[t].resize(ns);
      set_names_[t].resize(ns);

      for (unsigned int s=0; s<ns; s++)
      {
         unsigned int ni = mesh.ent_sets->GetNumEntities(t, s);

         sets_[t][s].resize(ni);
         set_names_[t][s] = mesh.ent_sets->GetSetName(t, s);
         set_index_by_name_[t][set_names_[t][s]] = s;

         for (unsigned int i=0; i<ni; i++)
         {
            sets_[t][s][i] = mesh.ent_sets->GetEntityIndex(t, s, i);
         }
      }
   }
   cout << "Leaving NCEntitySets(Mesh) c'tor" << endl;
}

NCEntitySets::NCEntitySets(const NCEntitySets & ncent_sets)
   : ncmesh_(ncent_sets.ncmesh_),
     sets_(4),
     set_names_(4),
     set_index_by_name_(4),
     entity_size_(4)
{
   cout << "Entering NCEntitySets copy c'tor" << endl;
   cout << "Leaving NCEntitySets copy c'tor" << endl;
}

unsigned int
NCEntitySets::GetNumSets(EntitySets::EntityType t) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");
   return sets_[t].size();
}

int
NCEntitySets::GetEntitySize(EntitySets::EntityType t) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");
   return entity_size_[t];
}

const string &
NCEntitySets::GetSetName(EntitySets::EntityType t, int s) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");
   return set_names_[t][s];
}

int
NCEntitySets::GetSetIndex(EntitySets::EntityType t, const string & s) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");

   map<string,int>::const_iterator mit = set_index_by_name_[t].find(s);

   MFEM_ASSERT( mit != set_index_by_name_[t].end(),
                "NCEntitySets unrecognized set name \"" << s
                << "\" for entity type \"" << t << "\"" );

   return mit->second;
}

unsigned int
NCEntitySets::GetNumEntities(EntitySets::EntityType t, int s) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");
   return sets_[t][s].size() / entity_size_[t];
}

unsigned int
NCEntitySets::GetNumEntities(EntitySets::EntityType t,
                             const std::string & s) const
{
   return this->GetNumEntities(t, this->GetSetIndex(t, s));
}

void
NCEntitySets::GetEntityIndex(EntitySets::EntityType t, int s, int i,
                             Array<int> & inds) const
{
   MFEM_ASSERT( t >= EntitySets::VERTEX && t <= EntitySets::ELEMENT,
                "NCEntitySets Invalid entity type \"" << t << "\"");

   inds.SetSize(entity_size_[t]);
   for (int j=0; j<entity_size_[t]; j++)
   {
      inds[j] = sets_[t][s][entity_size_[t] * i + j];
   }
}

void
NCEntitySets::GetEntityIndex(EntitySets::EntityType t, const string & s,
                             int i, Array<int> &inds) const
{
   return this->GetEntityIndex(t, this->GetSetIndex(t, s), i, inds);
}

}; // namespace mfem
