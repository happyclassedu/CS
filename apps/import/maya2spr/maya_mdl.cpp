/*
  Crystal Space Maya .ma Convertor
  Copyright (C) 2002 by Keith Fulton <keith@paqrat.com>
    (loosely based on "mdl2spr" by Nathaniel Saint Martin <noote@bigfoot.com>
                     and Eric Sunshine <sunshine@sunshineco.com>)

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cssysdef.h"
#include "maya_mdl.h"
#include "mayanode.h"

#include "cssys/csendian.h"
#include "iutil/databuff.h"
#include "igraphic/imageio.h"

extern csArray<Animation*> sockets;

bool Maya4Model::IsFileMayaModel(const char* mdlfile)
{
  return superclass::CheckMagic(mdlfile, "//Maya ASCII 4.0 scene");
}

Maya4Model::Maya4Model() : MayaModel()
{
  Clear();
}

Maya4Model::Maya4Model(const char* mdlfile)
{
  Clear();
  ReadMAFile(mdlfile);
}

Maya4Model::~Maya4Model()
{
  // @@@ FIXME: Unimplemented :-)
}

void Maya4Model::Clear()
{
    animnode = NULL;
    meshnode = NULL;
    filenode = NULL;
}

bool Maya4Model::ReadMAFile(const char* mdlfile)
{
    clearError();

    if (mdlfile == NULL || strlen(mdlfile) == 0)
        return setError("MA filename is NULL");

    MayaInputFile file(mdlfile);

    if (!file.IsValid())
        return setError("MA filename is NULL");

    while (!file.EndOfFile())
    {
        csString   tok;
        file.GetToken(tok);

        if (tok == "createNode")
        {
            if (!CreateNode(file,tree))
            {
                this->setError(file.GetError() );
                break;
            }
        }
    }

    // If file has loaded with no anim curves, create a default 0 anim curve
    if (!animnode)
    {
	printf("Creating default animcurveTL...\n");
        animnode = new NodeAnimCurveTL(meshnode->CountVertices()); // position paths for each vertex
	animnode->CreateDefault();
    }
    return true;
}

bool Maya4Model::CreateNode(MayaInputFile& file,DAGNode& tree)
{
    DAGNode *newnode=NULL;
    static int countanims = 0;
    static int vertices = 0;

    csString tok;
    file.GetToken(tok);

    if (tok == "transform")
        newnode = new NodeTransform;        // positioning of mesh
    else if (tok == "mesh")
    {
	printf("Mesh will be translated by (%1.2f,%1.2f,%1.2f)...\n",translate.x,translate.y,translate.z);
	printf("Mesh will be scaled by (%1.2f,%1.2f,%1.2f)...\n",scale.x,scale.y,scale.z);
        meshnode = new NodeMesh(translate,scale); // vertices, edges, polys
        newnode = meshnode;
    }
    else if (tok == "file")
    {
        filenode = new NodeFile;            // texture
        newnode  = filenode;
    }
    else if (tok == "animCurveTL")
    {
        if (!animnode)
            animnode = new NodeAnimCurveTL(vertices); // position paths for each vertex
        countanims++;
        newnode = animnode;  // reuse existing if already there
    }
    else
        return true;

    if (!newnode->Load(file))
        return false;

    static DAGNode *where=NULL;             // Use last specified parent if none specified

    if (newnode == animnode && countanims>1)
        return true;        // don't add more than once

    if (newnode->IsType("NodeMesh"))
    {
        NodeMesh *mesh = (NodeMesh *)newnode;
        vertices = mesh->CountVertices();
    }
    else if (newnode->IsType("NodeTransform"))
    {
	NodeTransform *tr = (NodeTransform *)newnode;
	tr->GetTransform(translate);
	tr->GetScale(scale);
    }

    if (newnode->Parent())
        where = tree.Find(newnode->Parent());

    if (where)
    {
        where->AddChild(newnode);
    }
    else
        tree.AddChild(newnode);  // add to top level of graph if no parent node

    return true;
}


void Maya4Model::dumpstats(FILE* s) 
{
  fprintf(s, "\nMaya Model (MDL) Statistics:\n");
  
  tree.PrintStats(s,0);
}

bool Maya4Model::WriteSPR(const char* spritename, csArray<Animation*>& anims) 
{
  FILE *f;
  Maya4Model spr;

  if (spritename == NULL || strlen(spritename) == 0)
  {
    fprintf(stderr, "Unable to save: NULL sprite name\n");
    return false;
  }

  printf("Writing out SPR file: %s\n",spritename);

  f = fopen(spritename, "w");
  if (!f)
  {
      fprintf(stderr,"Unable to open file (%s) to write out CS sprite.\n",spritename);
  }

    fprintf(f,"<meshfact>\n");
    fprintf(f,"   <plugin>crystalspace.mesh.loader.factory.sprite.3d</plugin>\n");
    fprintf(f,"   <params>\n");
    
    if (filenode)
        filenode->Write(f);
    else
	fprintf(f,"      <material>no_maya_material</material>\n");

    for (int frame=0; frame<((animnode)?animnode->GetFrames():1); frame++)
    {
        printf("Processing frame %d...\n",frame+1);

        if (frame>0)
            meshnode->ClearCS();

        // Move vertices for each frame
        meshnode->ApplyAnimDeltas(animnode,frame);

        // Fixups must be done on a frame by frame basis
        meshnode->FixupUniqueVertexMappings();

        fprintf(f,"      <frame name=\"f%d\">\n",frame+1);
        meshnode->WriteVertices(f);
        fprintf(f,"      </frame>\n");
    }

    // Now write out animation actions
    if (!anims.Length())
    {
      setError("At least one action animation is required.");    
      return false;
    }

    int i;
    for (i=0;i<anims.Length();i++)
    {
      Animation* anim = anims[i];
      
        int stop,start = anim->startframe;
        csString name = anim->name;
	int frame_duration = anim->duration;
	Animation *curr_anim = anim;

        printf("Writing out Animation %s\n",(const char *)name);

        if (i != anims.Length()-1)
            stop = anim->startframe-1;
        else
            stop = (animnode)?animnode->GetFrames():start;
	
	if (stop>start)
	    frame_duration /= (stop-start);
	else
	    frame_duration = 1000;

	if (!frame_duration)
	    frame_duration = 67; // default is 15 fps.

	// now determine displacement to use
	DisplacementGroup &dg = curr_anim->displacements[0];
	int displacementnum = 1;

        fprintf(f,"     <action name=\"%s\">\n",(const char *)name);

        for (int i=start; i<=stop; i++)
        {
	    if (i<dg.startframe || i>dg.stopframe)
	    {
                fprintf(f,"       <f name=\"f%d\" delay=\"%d\" />\n",i,frame_duration);
	    }
	    else
	    {
		int frame2 = (i+1>stop)?start:i+1;
		float displacement = meshnode->GetDisplacement(animnode, i-1,
		    frame2-1, dg.vertex);
		fprintf(f,"       <f name=\"f%d\" displacement=\"%f\" />\n",i,displacement);
		if (i==dg.stopframe) // get next displacement group
		{
		    dg = curr_anim->displacements[displacementnum++];
		}
	    }
        }

        fprintf(f,"     </action>\n");
    }

    printf("Writing out Triangles.\n");

    // Now write out all triangles
    meshnode->WriteTriangles(f);
    
    // Now write out sockets
    for (i=0;i<sockets.Length();i++)
    {
      Animation* socket = sockets[i];
      
      csString name = socket->name;
      int      tri  = socket->startframe; // really triangle #

      printf("Writing out Socket %s Tri %d\n",(const char *)name,tri);

      fprintf(f,"     <socket name=\"%s\" tri=\"%d\" />\n",(const char *)name, tri);
    }

    // Wrap up the file
    fprintf(f,"    </params>\n</meshfact>\n");

    fclose(f);

    return true;
}
