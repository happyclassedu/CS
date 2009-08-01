/*
    Copyright (C) 2005 by Jorrit Tyberghein

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "cssysdef.h"

#include "csgeom/math3d.h"
#include "csgeom/tri.h"
#include "csgeom/vector2.h"
#include "csgeom/vector4.h"
#include "csgeom/sphere.h"
#include "cstool/primitives.h"
#include "csutil/cscolor.h"
#include "csutil/dirtyaccessarray.h"
#include "csutil/scanstr.h"
#include "csutil/sysfunc.h"

#include "iengine/engine.h"
#include "iengine/material.h"
#include "iengine/mesh.h"
#include "imap/ldrctxt.h"
#include "imap/services.h"
#include "imesh/instmesh.h"
#include "imesh/object.h"
#include "iutil/comp.h"
#include "iutil/document.h"
#include "iutil/eventh.h"
#include "iutil/object.h"
#include "iutil/objreg.h"
#include "iutil/plugin.h"
#include "ivaria/reporter.h"
#include "ivideo/graph3d.h"
#include "ivideo/rndbuf.h"

#include "instmeshldr.h"

CS_IMPLEMENT_PLUGIN

enum
{
  XMLTOKEN_BOX = 1,
  XMLTOKEN_SPHERE,
  XMLTOKEN_LIGHTING,
  XMLTOKEN_DEFAULTCOLOR,
  XMLTOKEN_MATERIAL,
  XMLTOKEN_FACTORY,
  XMLTOKEN_MIXMODE,
  XMLTOKEN_MANUALCOLORS,
  XMLTOKEN_COLOR,
  XMLTOKEN_V,
  XMLTOKEN_T,
  XMLTOKEN_COLORS,
  XMLTOKEN_AUTONORMALS,
  XMLTOKEN_NORMALNOCOMPRESS,
  XMLTOKEN_NOSHADOWS,
  XMLTOKEN_LOCALSHADOWS,
  XMLTOKEN_COMPRESS,
  XMLTOKEN_INSTANCE
};

SCF_IMPLEMENT_FACTORY (csInstFactoryLoader)
SCF_IMPLEMENT_FACTORY (csInstFactorySaver)
SCF_IMPLEMENT_FACTORY (csInstMeshLoader)
SCF_IMPLEMENT_FACTORY (csInstMeshSaver)

csInstFactoryLoader::csInstFactoryLoader (iBase* pParent) :
  scfImplementationType(this, pParent)
{
}

csInstFactoryLoader::~csInstFactoryLoader ()
{
}

bool csInstFactoryLoader::Initialize (iObjectRegistry* object_reg)
{
  csInstFactoryLoader::object_reg = object_reg;
  reporter = csQueryRegistry<iReporter> (object_reg);
  synldr = csQueryRegistry<iSyntaxService> (object_reg);

  xmltokens.Register ("box", XMLTOKEN_BOX);
  xmltokens.Register ("sphere", XMLTOKEN_SPHERE);
  xmltokens.Register ("material", XMLTOKEN_MATERIAL);
  xmltokens.Register ("factory", XMLTOKEN_FACTORY);
  xmltokens.Register ("v", XMLTOKEN_V);
  xmltokens.Register ("t", XMLTOKEN_T);
  xmltokens.Register ("autonormals", XMLTOKEN_AUTONORMALS);
  xmltokens.Register ("normalnocompress", XMLTOKEN_NORMALNOCOMPRESS);

  xmltokens.Register ("mixmode", XMLTOKEN_MIXMODE);
  xmltokens.Register ("manualcolors", XMLTOKEN_MANUALCOLORS);
  xmltokens.Register ("defaultcolor", XMLTOKEN_DEFAULTCOLOR);
  xmltokens.Register ("lighting", XMLTOKEN_LIGHTING);
  xmltokens.Register ("noshadows", XMLTOKEN_NOSHADOWS);
  xmltokens.Register ("localshadows", XMLTOKEN_LOCALSHADOWS);
  return true;
}

static float GetDef (iDocumentNode* node, const char* attrname, float def)
{
  csRef<iDocumentAttribute> attr = node->GetAttribute (attrname);
  if (attr)
    return attr->GetValueAsFloat ();
  else
    return def;
}

static void AppendOrSetData (iInstancingFactoryState* factory,
    const csDirtyAccessArray<csVector3>& mesh_vertices,
    const csDirtyAccessArray<csVector2>& mesh_texels,
    const csDirtyAccessArray<csVector3>& mesh_normals,
    const csDirtyAccessArray<csTriangle>& mesh_triangles)
{
  csColor4 black (0, 0, 0);
  size_t cur_vt_count = factory->GetVertexCount ();
  size_t i;
  for (i = 0 ; i < mesh_vertices.GetSize () ; i++)
    factory->AddVertex (mesh_vertices[i], mesh_texels[i],
	  mesh_normals[i], black);
  for (i = 0 ; i < mesh_triangles.GetSize () ; i++)
  {
    csTriangle tri = mesh_triangles[i];
    tri.a += cur_vt_count;
    tri.b += cur_vt_count;
    tri.c += cur_vt_count;
    factory->AddTriangle (tri);
  }
}

csPtr<iBase> csInstFactoryLoader::Parse (iDocumentNode* node,
	iStreamSource*, iLoaderContext* ldr_context, iBase* /* context */)
{
  csRef<iMeshObjectType> type = csLoadPluginCheck<iMeshObjectType> (
  	object_reg, "crystalspace.mesh.object.instmesh", false);
  if (!type)
  {
    synldr->ReportError (
		"crystalspace.instmeshfactoryloader.setup.objecttype",
		node, "Could not load the general mesh object plugin!");
    return 0;
  }
  csRef<iMeshObjectFactory> fact;
  csRef<iInstancingFactoryState> state;

  fact = type->NewFactory ();
  state = scfQueryInterface<iInstancingFactoryState> (fact);

  bool auto_normals = false;
  bool auto_normals_nocompress = false;
  bool compress = false;

  csRef<iDocumentNodeIterator> it = node->GetNodes ();
  while (it->HasNext ())
  {
    csRef<iDocumentNode> child = it->Next ();
    if (child->GetType () != CS_NODE_ELEMENT) continue;
    const char* value = child->GetValue ();
    csStringID id = xmltokens.Request (value);
    switch (id)
    {
      case XMLTOKEN_MANUALCOLORS:
	{
	  bool r;
	  if (!synldr->ParseBool (child, r, true))
	    return 0;
	  state->SetManualColors (r);
	}
	break;
      case XMLTOKEN_NOSHADOWS:
	{
	  state->SetShadowCasting (false);
	}
	break;
      case XMLTOKEN_LOCALSHADOWS:
	{
	  state->SetShadowReceiving (true);
	}
	break;
      case XMLTOKEN_LIGHTING:
	{
	  bool r;
	  if (!synldr->ParseBool (child, r, true))
	    return 0;
	  state->SetLighting (r);
	}
	break;
      case XMLTOKEN_DEFAULTCOLOR:
	{
	  csColor col;
	  if (!synldr->ParseColor (child, col))
	    return 0;
	  state->SetColor (col);
	}
	break;
      case XMLTOKEN_MIXMODE:
        {
	  uint mm;
	  if (!synldr->ParseMixmode (child, mm))
	    return 0;
          fact->SetMixMode (mm);
	}
	break;
      case XMLTOKEN_MATERIAL:
	{
	  const char* matname = child->GetContentsValue ();
          iMaterialWrapper* mat = ldr_context->FindMaterial (matname);
	  if (!mat)
	  {
      	    synldr->ReportError (
		"crystalspace.instmeshfactoryloader.parse.unknownmaterial",
		child, "Couldn't find material '%s'!", matname);
            return 0;
	  }
	  fact->SetMaterialWrapper (mat);
	}
	break;
      case XMLTOKEN_BOX:
        {
	  using namespace CS::Geometry;
          csBox3 box;
          if (!synldr->ParseBox (child, box))
            return 0;
	  csDirtyAccessArray<csVector3> mesh_vertices;
	  csDirtyAccessArray<csVector2> mesh_texels;
	  csDirtyAccessArray<csVector3> mesh_normals;
	  csDirtyAccessArray<csTriangle> mesh_triangles;
	  Primitives::GenerateBox (box, mesh_vertices, mesh_texels,
	      mesh_normals, mesh_triangles, 0, 0);
	  AppendOrSetData (state, mesh_vertices, mesh_texels,
	    mesh_normals, mesh_triangles);
        }
        break;
      case XMLTOKEN_SPHERE:
        {
	  using namespace CS::Geometry;
          csVector3 center (0, 0, 0);
          int rim_vertices = 8;
          csEllipsoid ellips;
          csRef<iDocumentAttribute> attr;
          csRef<iDocumentNode> c = child->GetNode ("center");
          if (c)
            if (!synldr->ParseVector (c, ellips.GetCenter ()))
              return 0;
          c = child->GetNode ("radius");
          if (c)
          {
            if (!synldr->ParseVector (c, ellips.GetRadius ()))
              return 0;
          }
          else
          {
            attr = child->GetAttribute ("radius");
            float radius;
            if (attr) radius = attr->GetValueAsFloat ();
            else radius = 1.0f;
            ellips.SetRadius (csVector3 (radius, radius, radius));
          }
          attr = child->GetAttribute ("rimvertices");
          if (attr) rim_vertices = attr->GetValueAsInt ();
          bool cylmapping, toponly, reversed;
          if (!synldr->ParseBoolAttribute (child, "cylindrical", cylmapping,
              false, false))
            return 0;
          if (!synldr->ParseBoolAttribute (child, "toponly", toponly,
              false, false))
            return 0;
          if (!synldr->ParseBoolAttribute (child, "reversed", reversed,
              false, false))
            return 0;
	  csDirtyAccessArray<csVector3> mesh_vertices;
	  csDirtyAccessArray<csVector2> mesh_texels;
	  csDirtyAccessArray<csVector3> mesh_normals;
	  csDirtyAccessArray<csTriangle> mesh_triangles;
	  Primitives::GenerateSphere (ellips, rim_vertices,
	      mesh_vertices, mesh_texels,
	      mesh_normals, mesh_triangles, cylmapping,
	      toponly, reversed, 0);
	  AppendOrSetData (state, mesh_vertices, mesh_texels,
	    mesh_normals, mesh_triangles);
        }
        break;
      case XMLTOKEN_AUTONORMALS:
        if (!synldr->ParseBool (child, auto_normals, true))
	  return 0;
	break;
      case XMLTOKEN_NORMALNOCOMPRESS:
        if (!synldr->ParseBool (child, auto_normals_nocompress, true))
	  return 0;
      case XMLTOKEN_COMPRESS:
        if (!synldr->ParseBool (child, compress, true))
	  return 0;
	break;
      case XMLTOKEN_T:
	{
	  csTriangle tr;
	  tr.a = child->GetAttributeValueAsInt ("v1");
	  tr.b = child->GetAttributeValueAsInt ("v2");
	  tr.c = child->GetAttributeValueAsInt ("v3");
	  int vtcount = int (state->GetVertexCount ());
	  if (tr.a >= vtcount ||
	      tr.b >= vtcount ||
	      tr.c >= vtcount)
	  {
	    synldr->ReportError (
		    "crystalspace.instmeshfactoryloader.parse.frame.badvt",
		    child, "Bad vertex index for triangle in instmesh factory!"
		    );
	    return 0;
	  }
	  state->AddTriangle (tr);
	}
        break;
      case XMLTOKEN_V:
        {
	  csVector3 v, n;
	  csVector2 uv;
	  csColor4 col;
	  v.x = child->GetAttributeValueAsFloat ("x");
	  v.y = child->GetAttributeValueAsFloat ("y");
	  v.z = child->GetAttributeValueAsFloat ("z");
	  uv.x = child->GetAttributeValueAsFloat ("u");
	  uv.y = child->GetAttributeValueAsFloat ("v");
	  n.x = GetDef (child, "nx", 0.0f);
	  n.y = GetDef (child, "ny", 0.0f);
	  n.z = GetDef (child, "nz", 0.0f);
	  col.red = GetDef (child, "red", 0.0f);
	  col.green = GetDef (child, "green", 0.0f);
	  col.blue = GetDef (child, "blue", 0.0f);
	  col.alpha = GetDef (child, "alpha", 1.0f);
	  state->AddVertex (v, uv, n, col);
	}
        break;
      default:
	synldr->ReportBadToken (child);
	return 0;
    }
  }

  if (compress)
    state->Compress ();
  if (auto_normals)
    state->CalculateNormals (!auto_normals_nocompress);

  return csPtr<iBase> (fact);
}

//---------------------------------------------------------------------------

csInstFactorySaver::csInstFactorySaver (iBase* pParent) :
  scfImplementationType(this, pParent)
{
}

csInstFactorySaver::~csInstFactorySaver ()
{
}

bool csInstFactorySaver::Initialize (iObjectRegistry* object_reg)
{
  csInstFactorySaver::object_reg = object_reg;
  reporter = csQueryRegistry<iReporter> (object_reg);
  synldr = csQueryRegistry<iSyntaxService> (object_reg);
  return true;
}

bool csInstFactorySaver::WriteDown (iBase* obj, iDocumentNode* parent,
	iStreamSource*)
{
  if (!parent) return false; //you never know...
  
  csRef<iDocumentNode> paramsNode = 
    parent->CreateNodeBefore(CS_NODE_ELEMENT, 0);
  paramsNode->SetValue("params");

  if (obj)
  {
    csRef<iInstancingFactoryState> gfact = 
      scfQueryInterface<iInstancingFactoryState> (obj);
    csRef<iMeshObjectFactory> meshfact = 
      scfQueryInterface<iMeshObjectFactory> (obj);
    if (!gfact) return false;
    if (!meshfact) return false;

    //Write NumVt Tag
    csRef<iDocumentNode> numvtNode = 
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
    numvtNode->SetValue("numvt");
    numvtNode->CreateNodeBefore(CS_NODE_TEXT, 0)
      ->SetValueAsInt((int)gfact->GetVertexCount());

    //Write NumTri Tag
    csRef<iDocumentNode> numtriNode = 
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
    numtriNode->SetValue("numtri");
    numtriNode->CreateNodeBefore(CS_NODE_TEXT, 0)
      ->SetValueAsInt((int)gfact->GetTriangleCount());

    size_t i;
    //Write Vertex Tags
    for (i=0; i<gfact->GetVertexCount(); i++)
    {
      csRef<iDocumentNode> triaNode = 
        paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
      triaNode->SetValue("v");
      csVector3 vertex = gfact->GetVertices()[i];
      csVector2 texel = gfact->GetTexels()[i];
      csVector3 normal = gfact->GetNormals()[i];
      csColor4 color = gfact->GetColors()[i];
      synldr->WriteVector(triaNode, vertex);
      triaNode->SetAttributeAsFloat("u", texel.x);
      triaNode->SetAttributeAsFloat("v", texel.y);
      triaNode->SetAttributeAsFloat("nx", normal.x);
      triaNode->SetAttributeAsFloat("ny", normal.y);
      triaNode->SetAttributeAsFloat("nz", normal.z);
      triaNode->SetAttributeAsFloat("red", color.red);
      triaNode->SetAttributeAsFloat("green", color.green);
      triaNode->SetAttributeAsFloat("blue", color.blue);
      triaNode->SetAttributeAsFloat("alpha", color.alpha);
    }

    //Write Triangle Tags
    for (i=0; i<gfact->GetTriangleCount(); i++)
    {
      csRef<iDocumentNode> triaNode = 
        paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
      triaNode->SetValue("t");
      csTriangle tria = gfact->GetTriangles()[i];
      triaNode->SetAttributeAsInt("v1", tria.a);
      triaNode->SetAttributeAsInt("v2", tria.b);
      triaNode->SetAttributeAsInt("v3", tria.c);
    }

    //Writedown DefaultColor tag
    csColor col = gfact->GetColor();
    if (col.red != 0 || col.green != 0 || col.blue != 0)
    {
      csRef<iDocumentNode> colorNode = 
        paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
      colorNode->SetValue("defaultcolor");
      synldr->WriteColor(colorNode, col);
    }

    //Writedown Material tag
    iMaterialWrapper* mat = meshfact->GetMaterialWrapper();
    if (mat)
    {
      const char* matname = mat->QueryObject()->GetName();
      if (matname && *matname)
      {
        csRef<iDocumentNode> matNode = 
          paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
        matNode->SetValue("material");
        csRef<iDocumentNode> matnameNode = 
          matNode->CreateNodeBefore(CS_NODE_TEXT, 0);
        matnameNode->SetValue(matname);
      }    
    }    

    //Writedown Mixmode tag
    int mixmode = meshfact->GetMixMode();
    csRef<iDocumentNode> mixmodeNode = 
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
    mixmodeNode->SetValue("mixmode");
    synldr->WriteMixmode(mixmodeNode, mixmode, true);

    //Writedown Lighting tag
    synldr->WriteBool(paramsNode, "lighting", gfact->IsLighting(), true);

    //Writedown NoShadows tag
    if (!gfact->IsShadowCasting())
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)->SetValue("noshadows");

    //Writedown LocalShadows tag
    if (gfact->IsShadowReceiving())
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)
        ->SetValue("localshadows");

    //Writedown ManualColor tag
    if (gfact->IsManualColors())
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)
        ->SetValue("manualcolors");

    if (gfact->IsAutoNormals())
    {
      //Write Autonormals Tag
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)->SetValue("autonormals");
    }

    //TBD: Writedown box tag

    //TBD: Writedown renderbuffer tag
  }
  return true;
}

//---------------------------------------------------------------------------

csInstMeshLoader::csInstMeshLoader (iBase* pParent) :
  scfImplementationType(this, pParent)
{
}

csInstMeshLoader::~csInstMeshLoader ()
{
}

bool csInstMeshLoader::Initialize (iObjectRegistry* object_reg)
{
  csInstMeshLoader::object_reg = object_reg;
  reporter = csQueryRegistry<iReporter> (object_reg);
  synldr = csQueryRegistry<iSyntaxService> (object_reg);

  xmltokens.Register ("material", XMLTOKEN_MATERIAL);
  xmltokens.Register ("factory", XMLTOKEN_FACTORY);
  xmltokens.Register ("mixmode", XMLTOKEN_MIXMODE);
  xmltokens.Register ("color", XMLTOKEN_COLOR);
  xmltokens.Register ("manualcolors", XMLTOKEN_MANUALCOLORS);
  xmltokens.Register ("lighting", XMLTOKEN_LIGHTING);
  xmltokens.Register ("noshadows", XMLTOKEN_NOSHADOWS);
  xmltokens.Register ("localshadows", XMLTOKEN_LOCALSHADOWS);
  xmltokens.Register ("instance", XMLTOKEN_INSTANCE);
  return true;
}

#define CHECK_MESH(m) \
  if (!m) { \
    synldr->ReportError ( \
	"crystalspace.instmeshloader.parse.unknownfactory", \
	child, "Specify the factory first!"); \
    return 0; \
  }

csPtr<iBase> csInstMeshLoader::Parse (iDocumentNode* node,
	iStreamSource*, iLoaderContext* ldr_context, iBase*)
{
  csRef<iMeshObject> mesh;
  csRef<iInstancingMeshState> meshstate;
  csRef<iInstancingFactoryState> factstate;

  csRef<iDocumentNodeIterator> it = node->GetNodes ();
  while (it->HasNext ())
  {
    csRef<iDocumentNode> child = it->Next ();
    if (child->GetType () != CS_NODE_ELEMENT) continue;
    const char* value = child->GetValue ();
    csStringID id = xmltokens.Request (value);
    switch (id)
    {
      case XMLTOKEN_INSTANCE:
	{
	  csReversibleTransform trans;
	  csRef<iDocumentNode> matrix_node = child->GetNode ("matrix");
	  if (matrix_node)
	  {
	    csMatrix3 m;
	    if (!synldr->ParseMatrix (matrix_node, m))
	      return false;
            trans.SetO2T (m);
	  }
	  csRef<iDocumentNode> vector_node = child->GetNode ("v");
	  if (vector_node)
	  {
	    csVector3 v;
	    if (!synldr->ParseVector (vector_node, v))
	      return false;
            trans.SetO2TTranslation (v);
	  }
	  CHECK_MESH (meshstate);
	  meshstate->AddInstance (trans);
	}
	break;
      case XMLTOKEN_MANUALCOLORS:
	{
	  bool r;
	  if (!synldr->ParseBool (child, r, true))
	    return 0;
	  CHECK_MESH (meshstate);
	  meshstate->SetManualColors (r);
	}
	break;
      case XMLTOKEN_NOSHADOWS:
	{
	  CHECK_MESH (meshstate);
	  meshstate->SetShadowCasting (false);
	}
	break;
      case XMLTOKEN_LOCALSHADOWS:
	{
	  CHECK_MESH (meshstate);
	  meshstate->SetShadowReceiving (true);
	}
	break;
      case XMLTOKEN_LIGHTING:
	{
	  bool r;
	  if (!synldr->ParseBool (child, r, true))
	    return 0;
	  CHECK_MESH (meshstate);
	  meshstate->SetLighting (r);
	}
	break;
      case XMLTOKEN_COLOR:
	{
	  csColor col;
	  if (!synldr->ParseColor (child, col))
	    return 0;
	  CHECK_MESH (mesh);
	  mesh->SetColor (col);
	}
	break;
      case XMLTOKEN_FACTORY:
	{
	  const char* factname = child->GetContentsValue ();
	  iMeshFactoryWrapper* fact = ldr_context->FindMeshFactory (factname);
	  if (!fact)
	  {
      	    synldr->ReportError (
		"crystalspace.instmeshloader.parse.unknownfactory",
		child, "Couldn't find factory '%s'!", factname);
	    return 0;
	  }
	  factstate =  
	    scfQueryInterface<iInstancingFactoryState> (fact->GetMeshObjectFactory());
	  if (!factstate)
	  {
      	    synldr->ReportError (
		"crystalspace.instmeshloader.parse.badfactory",
		child, "Factory '%s' doesn't appear to be a instmesh factory!",
		factname);
	    return 0;
	  }
	  mesh = fact->GetMeshObjectFactory ()->NewInstance ();
	  CS_ASSERT (mesh != 0);
          meshstate = scfQueryInterface<iInstancingMeshState> (mesh);
	  if (!meshstate)
	  {
      	    synldr->ReportError (
		"crystalspace.instmeshloader.parse.badfactory",
		child, "Factory '%s' doesn't appear to be a instmesh factory!",
		factname);
	    return 0;
	  }
	}
	break;
      case XMLTOKEN_MATERIAL:
	{
	  const char* matname = child->GetContentsValue ();
          iMaterialWrapper* mat = ldr_context->FindMaterial (matname);
	  if (!mat)
	  {
      	    synldr->ReportError (
		"crystalspace.instmeshloader.parse.unknownmaterial",
		child, "Couldn't find material '%s'!", matname);
            return 0;
	  }
	  CHECK_MESH (mesh);
	  mesh->SetMaterialWrapper (mat);
	}
	break;
      case XMLTOKEN_MIXMODE:
        {
	  uint mm;
	  if (!synldr->ParseMixmode (child, mm))
	    return 0;
	  CHECK_MESH (mesh);
          mesh->SetMixMode (mm);
	}
	break;
      default:
        synldr->ReportBadToken (child);
	return 0;
    }
  }

  return csPtr<iBase> (mesh);
}

//---------------------------------------------------------------------------

csInstMeshSaver::csInstMeshSaver (iBase* pParent) :
  scfImplementationType(this, pParent)
{
}

csInstMeshSaver::~csInstMeshSaver ()
{
}

bool csInstMeshSaver::Initialize (iObjectRegistry* object_reg)
{
  csInstMeshSaver::object_reg = object_reg;
  reporter = csQueryRegistry<iReporter> (object_reg);
  synldr = csQueryRegistry<iSyntaxService> (object_reg);
  return true;
}

bool csInstMeshSaver::WriteDown (iBase* obj, iDocumentNode* parent,
	iStreamSource*)
{
  if (!parent) return false; //you never know...
  
  csRef<iDocumentNode> paramsNode = 
    parent->CreateNodeBefore(CS_NODE_ELEMENT, 0);
  paramsNode->SetValue("params");

  if (obj)
  {
    csRef<iInstancingMeshState> gmesh = 
      scfQueryInterface<iInstancingMeshState> (obj);
    csRef<iMeshObject> mesh = scfQueryInterface<iMeshObject> (obj);
    if (!gmesh) return false;
    if (!mesh) return false;

    //Writedown Factory tag
    iMeshFactoryWrapper* fact = mesh->GetFactory()->GetMeshFactoryWrapper ();
    if (fact)
    {
      const char* factname = fact->QueryObject()->GetName();
      if (factname && *factname)
      {
        csRef<iDocumentNode> factNode = 
          paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
        factNode->SetValue("factory");
        factNode->CreateNodeBefore(CS_NODE_TEXT, 0)->SetValue(factname);
      }    
    }

    //Writedown Lighting tag
    synldr->WriteBool(paramsNode, "lighting", gmesh->IsLighting(), true);

    //Writedown NoShadows tag
    if (!gmesh->IsShadowCasting())
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)->SetValue("noshadows");

    //Writedown LocalShadows tag
    if (!gmesh->IsShadowReceiving())
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0)
        ->SetValue("localshadows");

    //Writedown Color tag
    csColor col;
    mesh->GetColor(col);
    csRef<iDocumentNode> colorNode = 
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
    colorNode->SetValue("color");
    synldr->WriteColor(colorNode, col);

    //Writedown ManualColor tag
    synldr->WriteBool(paramsNode, "manualcolors",
                      gmesh->IsManualColors(), true);

    //Writedown Material tag
    iMaterialWrapper* mat = mesh->GetMaterialWrapper();
    if (mat)
    {
      const char* matname = mat->QueryObject()->GetName();
      if (matname && *matname)
      {
        csRef<iDocumentNode> matNode = 
          paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
        matNode->SetValue("material");
        csRef<iDocumentNode> matnameNode = 
          matNode->CreateNodeBefore(CS_NODE_TEXT, 0);
        matnameNode->SetValue(matname);
      }    
    }    

    //Writedown Mixmode tag
    int mixmode = mesh->GetMixMode();
    csRef<iDocumentNode> mixmodeNode = 
      paramsNode->CreateNodeBefore(CS_NODE_ELEMENT, 0);
    mixmodeNode->SetValue("mixmode");
    synldr->WriteMixmode(mixmodeNode, mixmode, true);
  }
  return true;
}