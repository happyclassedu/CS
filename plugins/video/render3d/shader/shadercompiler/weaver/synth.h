/*
  Copyright (C) 2007 by Frank Richter

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  Library General Public License for more details.

  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free
  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CS_SYNTH_H__
#define __CS_SYNTH_H__

#include "csplugincommon/shader/weavercombiner.h"
#include "csutil/blockallocator.h"
#include "csutil/hashr.h"
#include "csutil/set.h"
#include "csutil/strhash.h"

#include "snippet.h"
#include "weaver.h"

struct iDocumentNode;

CS_PLUGIN_NAMESPACE_BEGIN(ShaderWeaver)
{
  class WeaverCompiler;

  class ShaderVarNodesHelper
  {
    iDocumentNode* shaderVarsNode;
    csSet<csString> seenVars;
  public:
    ShaderVarNodesHelper (iDocumentNode* shaderVarsNode);
    
    void AddNode (iDocumentNode* node);
  };

  class Synthesizer
  {
    SnippetNumbers snipNums;
    csArray<csArray<TechniqueGraph> > graphs;
    csArray<Snippet*> outerSnippets;
  public:
    Synthesizer (WeaverCompiler* compiler, 
      const csPDelArray<Snippet>& outerSnippets);
    
    csPtr<iDocument> Synthesize (iDocumentNode* sourceNode);
  private:
    csString annotateString;
    const char* GetAnnotation (const char* fmt, ...) CS_GNUC_PRINTF (2, 3)
    {
      if (!compiler->annotateCombined) return 0;

      va_list args;
      va_start (args, fmt);
      annotateString.FormatV (fmt, args);
      va_end (args);
      return annotateString.GetData();
    }
    
    bool SynthesizeTechnique (
      ShaderVarNodesHelper& shaderVarNodes, iDocumentNode* passNode,
      const Snippet* snippet, const TechniqueGraph& graph);
    
    typedef csHash<csString, csString> StringStringHash;
    typedef csHashReversible<csString, csString> StringStringHashRev;
    class SynthesizeNodeTree
    {
    public:
      struct Node
      {
        csString annotation;
        const Snippet::Technique* tech;
        StringStringHash inputLinks;
        StringStringHash inputDefaults;
        StringStringHashRev outputRenames;
      };
      typedef csArray<Node*> NodeArray;
    private:
      size_t renameNr;
      csBlockAllocator<Node> nodeAlloc;
      NodeArray nodes;
      csHash<size_t, csConstPtrKey<Snippet::Technique> > techToNode;
      /// Techs created in an augmentation, to be cleaned up later
      csPDelArray<Snippet> scratchSnippets;
      csPDelArray<Snippet::Technique> augmentedTechniques;
      Synthesizer* synth;

      void ComputeRenames (Node& node,
        CS::PluginCommon::ShaderWeaver::iCombiner* combiner);
    public:
      SynthesizeNodeTree (Synthesizer* synth) : renameNr (0), synth (synth) {}
    
      void AddAllInputNodes (const TechniqueGraph& graph,
        const Snippet::Technique* tech,
        CS::PluginCommon::ShaderWeaver::iCombiner* combiner);
      void AugmentCoerceChain (WeaverCompiler* compiler, 
        const Snippet::Technique::CombinerPlugin& combinerPlugin,
        CS::PluginCommon::ShaderWeaver::iCombiner* combiner,
        CS::PluginCommon::ShaderWeaver::iCoerceChainIterator* linkChain, 
        TechniqueGraph& graph, Node& inNode, 
        const char* inName, const Snippet::Technique* outTech,
        const char* outName);
      Node& GetNodeForTech (const Snippet::Technique* tech)
      { return *nodes[techToNode.Get (tech, csArrayItemNotFound)]; }

      void ReverseNodeArray();
      
      void Rebuild (const TechniqueGraph& graph,
        const csArray<const Snippet::Technique*>& outputs);
        
      void Collapse (TechniqueGraph& graph);

      BasicIterator<Node>* GetNodes();
      BasicIterator<Node>* GetNodesReverse();
    private:
      template<bool reverse>
      class NodesIterator : public BasicIterator<Node>
      {
        const NodeArray& array;
        size_t nextItem;

        void SeekNext()
        {
          if (reverse)
          {
            while ((nextItem >= 1) && (array[nextItem-1]->tech == 0))
              nextItem--;
          }
          else
          {
            while ((nextItem < array.GetSize()) && (array[nextItem]->tech == 0))
              nextItem++;
          }
        }
      public:
        NodesIterator (const NodeArray& array) : array (array),
          nextItem (reverse ? array.GetSize() : 0)
        {
          SeekNext();
        }

        bool HasNext()
        { return reverse ? (nextItem > 0) : (nextItem < array.GetSize()); }
        Node& Next()
        { 
          Node* val = reverse ? array[--nextItem] : array[nextItem++];
          SeekNext();
          return *val; 
        }
	size_t GetTotal() const
	{
	  CS_ASSERT(false);
	  return array.GetSize(); 
	}
      };
    };

    bool FindOutput (const TechniqueGraph& graph,
      const char* desiredType,
      CS::PluginCommon::ShaderWeaver::iCombiner* combiner,
      const Snippet::Technique*& outTechnique,
      Snippet::Technique::Output& theOutput);
  
    typedef csSet<csConstPtrKey<Snippet::Technique::Output> > UsedOutputsHash;
    bool FindInput (const TechniqueGraph& graph,
      CS::PluginCommon::ShaderWeaver::iCombiner* combiner,
      csString& nodeAnnotation,
      const Snippet::Technique* receivingTech, 
      const Snippet::Technique::Input& input,
      const Snippet::Technique*& sourceTech,
      const Snippet::Technique::Output*& output,
      UsedOutputsHash& usedOutputs);
    bool FindExplicitInput (const TechniqueGraph& graph,
      CS::PluginCommon::ShaderWeaver::iCombiner* combiner,
      csString& nodeAnnotation,
      const Snippet::Technique* receivingTech, 
      const Snippet::Technique::Input& input,
      const Snippet::Technique*& sourceTech,
      const Snippet::Technique::Output*& output,
      UsedOutputsHash& usedOutputs);
    CS::PluginCommon::ShaderWeaver::iCombiner* GetCombiner (
      CS::PluginCommon::ShaderWeaver::iCombiner* used, 
      const Snippet::Technique::CombinerPlugin& comb,
      const Snippet::Technique::CombinerPlugin& requested,
      const char* requestedName);

    csString GetInputTag (CS::PluginCommon::ShaderWeaver::iCombiner* combiner,
      const Snippet::Technique::CombinerPlugin& comb,
      const Snippet::Technique::CombinerPlugin& combTech,
      const Snippet::Technique::Input& input);
    /// Structure to track what default inputs to emit.
    struct EmittedInput
    {
      const SynthesizeNodeTree::Node* node;
      const Snippet::Technique::Input* input;
      csArray<csString> conditions;
      csString tag;
    };
  public:
    WeaverCompiler* compiler;
  
    csStringHash& xmltokens;
    csRef<CS::PluginCommon::ShaderWeaver::iCombiner> defaultCombiner;
  };

}
CS_PLUGIN_NAMESPACE_END(ShaderWeaver)

#endif // __CS_SYNTH_H__
