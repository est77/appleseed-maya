
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2016 Esteban Tovagliari, The appleseedhq Organization
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "appleseedmaya/exporters/shadingnetworkexporter.h"

// Maya headers.
#include <maya/MFnDependencyNode.h>
#include <maya/MItDependencyGraph.h>

// appleseed.renderer headers.
#include "renderer/api/scene.h"

// appleseed.maya headers.
#include "appleseedmaya/exceptions.h"
#include "appleseedmaya/exporters/exporterfactory.h"
#include "appleseedmaya/logger.h"
#include "appleseedmaya/shadingnoderegistry.h"

namespace asf = foundation;
namespace asr = renderer;

ShadingNetworkExporter::ShadingNetworkExporter(
    const Context                 context,
    const MObject&                object,
    const MPlug&                  plug,
    renderer::Assembly&           mainAssembly,
    AppleseedSession::SessionMode sessionMode)
  : m_context(context)
  , m_object(object)
  , m_mainAssembly(mainAssembly)
  , m_sessionMode(sessionMode)
{
}

MString ShadingNetworkExporter::shaderGroupName() const
{
    return m_shaderGroup->get_name();
}

void ShadingNetworkExporter::createEntity(const AppleseedSession::Options& options)
{
    MFnDependencyNode depNodeFn(m_object);
    m_shaderGroup = asr::ShaderGroupFactory::create(shaderGroupName().asChar());

    // Create adaptors, ..., here.
    switch(m_context)
    {
        case SurfaceContext:
        case DisplacementContext:
        case LightAttenuation:
        break;
    }

    MStatus status;
    MItDependencyGraph it(
        m_object,
        MFn::kInvalid,
        MItDependencyGraph::kUpstream,
        MItDependencyGraph::kDepthFirst,
        MItDependencyGraph::kNodeLevel,
        &status);

    if(status == MS::kFailure)
    {
        RENDERER_LOG_WARNING(
            "No nodes connected to shading node %s",
            depNodeFn.name().asChar());
        return;
    }

    bool networkHasUnknownNodes = false;

    // iterate through the output connected shading nodes.
    for(; it.isDone() != true; it.next())
    {
        depNodeFn.setObject(it.thisNode());
        if(m_nodeExporters.count(depNodeFn.name()) == 0)
        {
            //RENDERER_LOG_INFO("Visiting node %s", depNodeFn.name().asChar());

            ShadingNodeExporterPtr exporter;

            try
            {
                exporter.reset(NodeExporterFactory::createShadingNodeExporter(
                    depNodeFn.object(),
                    *m_shaderGroup));
            }
            catch(const NoExporterForNode&)
            {
                networkHasUnknownNodes = true;
                RENDERER_LOG_WARNING(
                    "No shading node exporter found for node type %s",
                    depNodeFn.typeName().asChar());
            }

            if(exporter)
            {
                m_nodeExporters[depNodeFn.name()] = exporter;
                RENDERER_LOG_DEBUG(
                    "Created shading node exporter for node %s",
                    depNodeFn.name().asChar());

                exporter->createShader();
            }
        }
    }
}

void ShadingNetworkExporter::flushEntity()
{
    insertEntityWithUniqueName(
        m_mainAssembly.shader_groups(),
        m_shaderGroup);
}