/*
 * Copyright (c) 2015-2023 The Khronos Group Inc.
 * Copyright (c) 2015-2023 Valve Corporation
 * Copyright (c) 2015-2023 LunarG, Inc.
 * Copyright (c) 2015-2023 Google, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 */

#include "../framework/layer_validation_tests.h"
#include "generated/vk_extension_helper.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#include "utils/cast_utils.h"

//
// POSITIVE VALIDATION TESTS
//
// These tests do not expect to encounter ANY validation errors pass only if this is true

TEST_F(VkPositiveLayerTest, CreatePipelineComplexTypes) {
    TEST_DESCRIPTION("Smoke test for complex types across VS/FS boundary");
    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    if (!m_device->phy().features().tessellationShader) {
        GTEST_SKIP() << "Device does not support tessellation shaders";
    }

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj tcs(this, bindStateTscShaderText, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    VkShaderObj tes(this, bindStateTeshaderText, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE};
    VkPipelineTessellationStateCreateInfo tsci{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, nullptr, 0, 3};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pTessellationState = &tsci;
    pipe.gp_ci_.pInputAssemblyState = &iasci;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), tcs.GetStageCreateInfo(), tes.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineAttribMatrixType) {
    TEST_DESCRIPTION("Test that pipeline validation accepts matrices passed as vertex attributes");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkVertexInputBindingDescription input_binding;
    memset(&input_binding, 0, sizeof(input_binding));

    VkVertexInputAttributeDescription input_attribs[2];
    memset(input_attribs, 0, sizeof(input_attribs));

    for (int i = 0; i < 2; i++) {
        input_attribs[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        input_attribs[i].location = i;
    }

    char const *vsSource = R"glsl(
        #version 450
        layout(location=0) in mat2x4 x;
        void main(){
           gl_Position = x[0] + x[1];
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.vi_ci_.pVertexBindingDescriptions = &input_binding;
    pipe.vi_ci_.vertexBindingDescriptionCount = 1;
    pipe.vi_ci_.pVertexAttributeDescriptions = input_attribs;
    pipe.vi_ci_.vertexAttributeDescriptionCount = 2;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
    /* expect success */
}

TEST_F(VkPositiveLayerTest, CreatePipelineAttribArrayType) {
    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkVertexInputBindingDescription input_binding;
    memset(&input_binding, 0, sizeof(input_binding));

    VkVertexInputAttributeDescription input_attribs[2];
    memset(input_attribs, 0, sizeof(input_attribs));

    for (int i = 0; i < 2; i++) {
        input_attribs[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        input_attribs[i].location = i;
    }

    char const *vsSource = R"glsl(
        #version 450
        layout(location=0) in vec4 x[2];
        void main(){
           gl_Position = x[0] + x[1];
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.vi_ci_.pVertexBindingDescriptions = &input_binding;
    pipe.vi_ci_.vertexBindingDescriptionCount = 1;
    pipe.vi_ci_.pVertexAttributeDescriptions = input_attribs;
    pipe.vi_ci_.vertexAttributeDescriptionCount = 2;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineAttribComponents) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts consuming a vertex attribute through multiple vertex shader inputs, each consuming "
        "a different subset of the components, and that fragment shader-attachment validation tolerates multiple duplicate "
        "location outputs");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkVertexInputBindingDescription input_binding;
    memset(&input_binding, 0, sizeof(input_binding));

    VkVertexInputAttributeDescription input_attribs[3];
    memset(input_attribs, 0, sizeof(input_attribs));

    for (int i = 0; i < 3; i++) {
        input_attribs[i].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        input_attribs[i].location = i;
    }

    char const *vsSource = R"glsl(
        #version 450
        layout(location=0) in vec4 x;
        layout(location=1) in vec3 y1;
        layout(location=1, component=3) in float y2;
        layout(location=2) in vec4 z;
        void main(){
           gl_Position = x + vec4(y1, y2) + z;
        }
    )glsl";
    char const *fsSource = R"glsl(
        #version 450
        layout(location=0, component=0) out float color0;
        layout(location=0, component=1) out float color1;
        layout(location=0, component=2) out float color2;
        layout(location=0, component=3) out float color3;
        layout(location=1, component=0) out vec2 second_color0;
        layout(location=1, component=2) out vec2 second_color1;
        void main(){
           color0 = float(1);
           second_color0 = vec2(1);
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineObj pipe(m_device);

    VkDescriptorSetObj descriptorSet(m_device);
    descriptorSet.AppendDummy();
    descriptorSet.CreateVKDescriptorSet(m_commandBuffer);

    // Create a renderPass with two color attachments
    VkAttachmentReference attachments[2] = {};
    attachments[0].layout = VK_IMAGE_LAYOUT_GENERAL;
    attachments[1].attachment = 1;
    attachments[1].layout = VK_IMAGE_LAYOUT_GENERAL;

    VkSubpassDescription subpass = {};
    subpass.pColorAttachments = attachments;
    subpass.colorAttachmentCount = 2;

    VkRenderPassCreateInfo rpci = LvlInitStruct<VkRenderPassCreateInfo>();
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    rpci.attachmentCount = 2;

    VkAttachmentDescription attach_desc[2] = {};
    attach_desc[0].format = VK_FORMAT_B8G8R8A8_UNORM;
    attach_desc[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attach_desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attach_desc[0].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    attach_desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attach_desc[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    attach_desc[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attach_desc[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attach_desc[1].finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    attach_desc[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;

    rpci.pAttachments = attach_desc;
    vk_testing::RenderPass renderpass(*m_device, rpci);
    pipe.AddShader(&vs);
    pipe.AddShader(&fs);

    VkPipelineColorBlendAttachmentState att_state1 = {};
    att_state1.dstAlphaBlendFactor = VK_BLEND_FACTOR_CONSTANT_COLOR;
    att_state1.blendEnable = VK_FALSE;

    pipe.AddColorAttachment(0, att_state1);
    pipe.AddColorAttachment(1, att_state1);
    pipe.AddVertexInputBindings(&input_binding, 1);
    pipe.AddVertexInputAttribs(input_attribs, 3);
    pipe.CreateVKPipeline(descriptorSet.GetPipelineLayout(), renderpass.handle());
}

TEST_F(VkPositiveLayerTest, CreatePipelineSimplePositive) {
    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineRelaxedTypeMatch) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts the relaxed type matching rules set out in VK_KHR_maintenance4 (default in Vulkan "
        "1.3) device extension:"
        "fundamental type must match, and producer side must have at least as many components");

    SetTargetApiVersion(VK_API_VERSION_1_1); // At least 1.1 is required for maintenance4
    AddRequiredExtensions(VK_KHR_MAINTENANCE_4_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan 1.1 is required";
    }
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " required but not supported";
    }
    auto maint4features = LvlInitStruct<VkPhysicalDeviceMaintenance4FeaturesKHR>();
    auto features2 = GetPhysicalDeviceFeatures2(maint4features);
    if (!maint4features.maintenance4) {
        GTEST_SKIP() << "VkPhysicalDeviceMaintenance4FeaturesKHR::maintenance4 is required but not enabled.";
    }
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *vsSource = R"glsl(
        #version 450
        layout(location=0) out vec3 x;
        layout(location=1) out ivec3 y;
        layout(location=2) out vec3 z;
        void main(){
           gl_Position = vec4(0);
           x = vec3(0); y = ivec3(0); z = vec3(0);
        }
    )glsl";
    char const *fsSource = R"glsl(
        #version 450
        layout(location=0) out vec4 color;
        layout(location=0) in float x;
        layout(location=1) flat in int y;
        layout(location=2) in vec2 z;
        void main(){
           color = vec4(1 + x + y + z.x);
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineTessPerVertex) {
    TEST_DESCRIPTION("Test that pipeline validation accepts per-vertex variables passed between the TCS and TES stages");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    if (!m_device->phy().features().tessellationShader) {
        GTEST_SKIP() << "Device does not support tessellation shaders";
    }

    char const *tcsSource = R"glsl(
        #version 450
        layout(location=0) out int x[];
        layout(vertices=3) out;
        void main(){
           gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = 1;
           gl_TessLevelInner[0] = 1;
           x[gl_InvocationID] = gl_InvocationID;
        }
    )glsl";
    char const *tesSource = R"glsl(
        #version 450
        layout(triangles, equal_spacing, cw) in;
        layout(location=0) in int x[];
        void main(){
           gl_Position.xyz = gl_TessCoord;
           gl_Position.w = x[0] + x[1] + x[2];
        }
    )glsl";

    VkShaderObj vs(this, bindStateMinimalShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj tcs(this, tcsSource, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    VkShaderObj tes(this, tesSource, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE};

    VkPipelineTessellationStateCreateInfo tsci{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, nullptr, 0, 3};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pTessellationState = &tsci;
    pipe.gp_ci_.pInputAssemblyState = &iasci;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), tcs.GetStageCreateInfo(), tes.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineGeometryInputBlockPositive) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts a user-defined interface block passed into the geometry shader. This is interesting "
        "because the 'extra' array level is not present on the member type, but on the block instance.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    if (!m_device->phy().features().geometryShader) {
        GTEST_SKIP() << "Device does not support geometry shaders";
    }

    char const *vsSource = R"glsl(
        #version 450

        layout(location = 0) out VertexData { vec4 x; } gs_out;

        void main(){
           gs_out.x = vec4(1.0f);
        }
    )glsl";

    char const *gsSource = R"glsl(
        #version 450
        layout(triangles) in;
        layout(triangle_strip, max_vertices=3) out;
        layout(location=0) in VertexData { vec4 x; } gs_in[];
        void main() {
           gl_Position = gs_in[0].x;
           EmitVertex();
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj gs(this, gsSource, VK_SHADER_STAGE_GEOMETRY_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), gs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipeline64BitAttributesPositive) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts basic use of 64bit vertex attributes. This is interesting because they consume "
        "multiple locations.");

    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    if (!m_device->phy().features().shaderFloat64) {
        GTEST_SKIP() << "Device does not support 64bit vertex attributes";
    }

    VkFormatProperties format_props;
    vk::GetPhysicalDeviceFormatProperties(gpu(), VK_FORMAT_R64G64B64A64_SFLOAT, &format_props);
    if (!(format_props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT)) {
        GTEST_SKIP() << "Device does not support VK_FORMAT_R64G64B64A64_SFLOAT vertex buffers";
    }

    VkVertexInputBindingDescription input_bindings[1];
    memset(input_bindings, 0, sizeof(input_bindings));

    VkVertexInputAttributeDescription input_attribs[4];
    memset(input_attribs, 0, sizeof(input_attribs));
    input_attribs[0].location = 0;
    input_attribs[0].offset = 0;
    input_attribs[0].format = VK_FORMAT_R64G64B64A64_SFLOAT;
    input_attribs[1].location = 2;
    input_attribs[1].offset = 32;
    input_attribs[1].format = VK_FORMAT_R64G64B64A64_SFLOAT;
    input_attribs[2].location = 4;
    input_attribs[2].offset = 64;
    input_attribs[2].format = VK_FORMAT_R64G64B64A64_SFLOAT;
    input_attribs[3].location = 6;
    input_attribs[3].offset = 96;
    input_attribs[3].format = VK_FORMAT_R64G64B64A64_SFLOAT;

    char const *vsSource = R"glsl(
        #version 450
        layout(location=0) in dmat4 x;
        void main(){
           gl_Position = vec4(x[0][0]);
        }
    )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.vi_ci_.pVertexBindingDescriptions = input_bindings;
    pipe.vi_ci_.vertexBindingDescriptionCount = 1;
    pipe.vi_ci_.pVertexAttributeDescriptions = input_attribs;
    pipe.vi_ci_.vertexAttributeDescriptionCount = 4;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineInputAttachment) {
    TEST_DESCRIPTION("Positive test for a correctly matched input attachment");

    ASSERT_NO_FATAL_FAILURE(Init());

    char const *fsSource = R"glsl(
        #version 450
        layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput x;
        layout(location=0) out vec4 color;
        void main() {
           color = subpassLoad(x);
        }
    )glsl";

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineObj pipe(m_device);
    pipe.AddShader(&vs);
    pipe.AddShader(&fs);
    pipe.AddDefaultColorAttachment();
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkDescriptorSetLayoutBinding dslb = {0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    const VkDescriptorSetLayoutObj dsl(m_device, {dslb});
    const VkPipelineLayoutObj pl(m_device, {&dsl});

    VkAttachmentDescription descs[2] = {
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL},
    };
    VkAttachmentReference color = {
        0,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    VkAttachmentReference input = {
        1,
        VK_IMAGE_LAYOUT_GENERAL,
    };

    VkSubpassDescription sd = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 1, &input, 1, &color, nullptr, nullptr, 0, nullptr};

    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 2, descs, 1, &sd, 0, nullptr};
    vk_testing::RenderPass rp(*m_device, rpci);

    // should be OK. would go wrong here if it's going to...
    pipe.CreateVKPipeline(pl.handle(), rp.handle());
}

TEST_F(VkPositiveLayerTest, CreatePipelineInputAttachmentMissingNotRead) {
    TEST_DESCRIPTION("Input Attachment would be missing, but it is not read from in shader");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput xs[1];
    // layout(location=0) out vec4 color;
    // void main() {
    //     // (not actually called) color = subpassLoad(xs[0]);
    // }
    const char *fsSource = R"(
               OpCapability Shader
               OpCapability InputAttachment
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main" %color
               OpExecutionMode %main OriginUpperLeft
               OpDecorate %color Location 0
               OpDecorate %xs DescriptorSet 0
               OpDecorate %xs Binding 0
               OpDecorate %xs InputAttachmentIndex 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
      %float = OpTypeFloat 32
    %v4float = OpTypeVector %float 4
%_ptr_Output_v4float = OpTypePointer Output %v4float
      %color = OpVariable %_ptr_Output_v4float Output
         %10 = OpTypeImage %float SubpassData 0 0 0 2 Unknown
       %uint = OpTypeInt 32 0
     %uint_1 = OpConstant %uint 1
%_arr_10_uint_1 = OpTypeArray %10 %uint_1
%_ptr_UniformConstant__arr_10_uint_1 = OpTypePointer UniformConstant %_arr_10_uint_1
         %xs = OpVariable %_ptr_UniformConstant__arr_10_uint_1 UniformConstant
        %int = OpTypeInt 32 1
      %int_0 = OpConstant %int 0
%_ptr_UniformConstant_10 = OpTypePointer UniformConstant %10
      %v2int = OpTypeVector %int 2
         %22 = OpConstantComposite %v2int %int_0 %int_0
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd)";

    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    const auto set_info = [&](CreatePipelineHelper &helper) {
        helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
        helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    };
    CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
}

TEST_F(VkPositiveLayerTest, CreatePipelineInputAttachmentArray) {
    TEST_DESCRIPTION("Input Attachment array where need to follow the index into the array");

    SetTargetApiVersion(VK_API_VERSION_1_2);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (DeviceValidationVersion() < VK_API_VERSION_1_2) {
        GTEST_SKIP() << "At least Vulkan version 1.2 is required";
    }
    auto features12 = LvlInitStruct<VkPhysicalDeviceVulkan12Features>();
    GetPhysicalDeviceFeatures2(features12);
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features12));

    const VkAttachmentDescription inputAttachmentDescription = {0,
                                                                m_render_target_fmt,
                                                                VK_SAMPLE_COUNT_1_BIT,
                                                                VK_ATTACHMENT_LOAD_OP_LOAD,
                                                                VK_ATTACHMENT_STORE_OP_STORE,
                                                                VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                                VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                                VK_IMAGE_LAYOUT_GENERAL,
                                                                VK_IMAGE_LAYOUT_GENERAL};

    // index 0 is unused
    // index 1 is is valid (for both color and input)
    // index 2 and 3 point to same image as index 1
    const VkAttachmentReference inputAttachmentReferences[4] = {{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_GENERAL},
                                                                {0, VK_IMAGE_LAYOUT_GENERAL},
                                                                {0, VK_IMAGE_LAYOUT_GENERAL},
                                                                {0, VK_IMAGE_LAYOUT_GENERAL}};

    const VkSubpassDescription subpassDescription = {(VkSubpassDescriptionFlags)0,
                                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     4,
                                                     inputAttachmentReferences,
                                                     1,
                                                     &inputAttachmentReferences[1],
                                                     nullptr,
                                                     nullptr,
                                                     0,
                                                     nullptr};

    auto renderPassInfo = LvlInitStruct<VkRenderPassCreateInfo>();
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &inputAttachmentDescription;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;

    vk_testing::RenderPass renderPass(*m_device, renderPassInfo);

    // use static array of 2 and index into element 1 to read
    {
        const char *fs_source = R"(
            #version 460
            layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput xs[2];
            layout(location=0) out vec4 color;
            void main() {
                color = subpassLoad(xs[1]);
            }
        )";
        VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            helper.gp_ci_.renderPass = renderPass.handle();
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // use undefined size array and index into element 1 to read
    {
        const char *fs_source = R"(
            #version 460
            layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput xs[];
            layout(location=0) out vec4 color;
            void main() {
                color = subpassLoad(xs[1]);
            }
        )";
        VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            helper.gp_ci_.renderPass = renderPass.handle();
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // use OpTypeRuntimeArray and index into it
    // This is something that is needed to be validated at draw time, so should not be an error
    if (features12.runtimeDescriptorArray && features12.shaderInputAttachmentArrayNonUniformIndexing) {
        const char *fs_source = R"(
            #version 460
            #extension GL_EXT_nonuniform_qualifier : require
            layout(input_attachment_index=0, set=0, binding=0) uniform subpassInput xs[];
            layout(set = 0, binding = 3) buffer ssbo { int rIndex; };
            layout(location=0) out vec4 color;
            void main() {
                color = subpassLoad(xs[nonuniformEXT(rIndex)]);
            }
        )";
        VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                                    {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            helper.gp_ci_.renderPass = renderPass.handle();
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // Array of size 1
    // loads from index 0, but not the invalid index 0 since has offest of 3
    {
        const char *fs_source = R"(
            #version 460
            layout(input_attachment_index=3, set=0, binding=0) uniform subpassInput xs[1];
            layout(location=0) out vec4 color;
            void main() {
                color = subpassLoad(xs[0]);
            }
        )";
        VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            helper.gp_ci_.renderPass = renderPass.handle();
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // Index from non-zero
    {
        const char *fs_source = R"(
            #version 460
            layout(input_attachment_index=2, set=0, binding=0) uniform subpassInput xs[2];
            layout(location=0) out vec4 color;
            void main() {
                color = subpassLoad(xs[0]) + subpassLoad(xs[1]);
            }
        )";
        VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
            helper.gp_ci_.renderPass = renderPass.handle();
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, CreatePipelineInputAttachmentDepthStencil) {
    TEST_DESCRIPTION("Input Attachment sharing same variable, but different aspect");

    SetTargetApiVersion(VK_API_VERSION_1_2);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (DeviceValidationVersion() < VK_API_VERSION_1_2) {
        GTEST_SKIP() << "At least Vulkan version 1.2 is required";
    }
    auto features12 = LvlInitStruct<VkPhysicalDeviceVulkan12Features>();
    GetPhysicalDeviceFeatures2(features12);
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features12));

    const VkFormat ds_format = FindSupportedDepthStencilFormat(gpu());

    const VkAttachmentDescription inputAttachmentDescriptions[2] = {
        {0, m_render_target_fmt, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL},
        {0, ds_format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL}};

    // index 0 = color | index 1 = depth | index 2 = stencil
    const VkAttachmentReference inputAttachmentReferences[3] = {
        {0, VK_IMAGE_LAYOUT_GENERAL}, {1, VK_IMAGE_LAYOUT_GENERAL}, {1, VK_IMAGE_LAYOUT_GENERAL}};

    const VkSubpassDescription subpassDescription = {(VkSubpassDescriptionFlags)0,
                                                     VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                     3,
                                                     inputAttachmentReferences,
                                                     1,
                                                     &inputAttachmentReferences[0],
                                                     nullptr,
                                                     nullptr,
                                                     0,
                                                     nullptr};

    auto renderPassInfo = LvlInitStruct<VkRenderPassCreateInfo>();
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = inputAttachmentDescriptions;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;

    vk_testing::RenderPass renderPass(*m_device, renderPassInfo);

    // Depth and Stencil use same index, but valid because differnet image aspect masks
    const char *fs_source = R"(
            #version 460
            layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput i_color;
            layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput i_depth;
            layout(input_attachment_index = 1, set = 0, binding = 2) uniform usubpassInput i_stencil;
            layout(location=0) out vec4 color;

            void main(void)
            {
                color = subpassLoad(i_color);
                vec4 depth = subpassLoad(i_depth);
                uvec4 stencil = subpassLoad(i_stencil);
            }
        )";
    VkShaderObj fs(this, fs_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL);

    const auto set_info = [&](CreatePipelineHelper &helper) {
        helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
        helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                                {1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                                {2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
        helper.gp_ci_.renderPass = renderPass.handle();
    };
    CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
}

TEST_F(VkPositiveLayerTest, CreateComputePipelineMissingDescriptorUnusedPositive) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts a compute pipeline which declares a descriptor-backed resource which is not "
        "provided, but the shader does not statically use it. This is interesting because it requires compute pipelines to have a "
        "proper descriptor use walk, which they didn't for some time.");

    ASSERT_NO_FATAL_FAILURE(Init());

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) buffer block { vec4 x; };
        void main(){
           // x is not used.
        }
    )glsl";

    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.CreateComputePipeline();
}

TEST_F(VkPositiveLayerTest, CreateComputePipelineFragmentShadingRate) {
    TEST_DESCRIPTION("Verify that pipeline validation accepts a compute pipeline with fragment shading rate extension enabled");

    AddRequiredExtensions(VK_KHR_MAINTENANCE_1_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto fsr_features = LvlInitStruct<VkPhysicalDeviceFragmentShadingRateFeaturesKHR>();
    VkPhysicalDeviceFeatures2 features2 = GetPhysicalDeviceFeatures2(fsr_features);
    if (fsr_features.pipelineFragmentShadingRate == VK_FALSE || fsr_features.primitiveFragmentShadingRate == VK_FALSE) {
        GTEST_SKIP() << "Test requires (unsupported) pipelineFragmentShadingRate and primitiveFragmentShadingRate";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) buffer block { vec4 x; };
        void main(){
           // x is not used.
        }
    )glsl";

    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.CreateComputePipeline();
}

TEST_F(VkPositiveLayerTest, CreateComputePipelineCombinedImageSamplerConsumedAsSampler) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts a shader consuming only the sampler portion of a combined image + sampler");

    ASSERT_NO_FATAL_FAILURE(Init());

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) uniform sampler s;
        layout(set=0, binding=1) uniform texture2D t;
        layout(set=0, binding=2) buffer block { vec4 x; };
        void main() {
           x = texture(sampler2D(t, s), vec2(0));
        }
    )glsl";
    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.dsl_bindings_.resize(bindings.size());
    memcpy(pipe.dsl_bindings_.data(), bindings.data(), bindings.size() * sizeof(VkDescriptorSetLayoutBinding));
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.CreateComputePipeline();
}

TEST_F(VkPositiveLayerTest, CreateComputePipelineCombinedImageSamplerConsumedAsImage) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts a shader consuming only the image portion of a combined image + sampler");

    ASSERT_NO_FATAL_FAILURE(Init());

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) uniform texture2D t;
        layout(set=0, binding=1) uniform sampler s;
        layout(set=0, binding=2) buffer block { vec4 x; };
        void main() {
           x = texture(sampler2D(t, s), vec2(0));
        }
    )glsl";
    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.dsl_bindings_.resize(bindings.size());
    memcpy(pipe.dsl_bindings_.data(), bindings.data(), bindings.size() * sizeof(VkDescriptorSetLayoutBinding));
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.CreateComputePipeline();
}

TEST_F(VkPositiveLayerTest, CreateComputePipelineCombinedImageSamplerConsumedAsBoth) {
    TEST_DESCRIPTION(
        "Test that pipeline validation accepts a shader consuming both the sampler and the image of a combined image+sampler but "
        "via separate variables");

    ASSERT_NO_FATAL_FAILURE(Init());

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
    };

    char const *csSource = R"glsl(
        #version 450
        layout(local_size_x=1) in;
        layout(set=0, binding=0) uniform texture2D t;
        layout(set=0, binding=0) uniform sampler s;  // both binding 0!
        layout(set=0, binding=1) buffer block { vec4 x; };
        void main() {
           x = texture(sampler2D(t, s), vec2(0));
        }
    )glsl";
    CreateComputePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.dsl_bindings_.resize(bindings.size());
    memcpy(pipe.dsl_bindings_.data(), bindings.data(), bindings.size() * sizeof(VkDescriptorSetLayoutBinding));
    pipe.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT));
    pipe.InitState();
    pipe.CreateComputePipeline();
}

TEST_F(VkPositiveLayerTest, PSOPolygonModeValid) {
    TEST_DESCRIPTION("Verify that using a solid polygon fill mode works correctly.");

    ASSERT_NO_FATAL_FAILURE(Init());
    if (IsPlatform(kNexusPlayer)) {
        GTEST_SKIP() << "This test should not run on Nexus Player";
    }
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    std::vector<const char *> device_extension_names;
    auto features = m_device->phy().features();
    // Artificially disable support for non-solid fill modes
    features.fillModeNonSolid = false;
    // The sacrificial device object
    VkDeviceObj test_device(0, gpu(), device_extension_names, &features);

    VkRenderpassObj render_pass(&test_device);

    const VkPipelineLayoutObj pipeline_layout(&test_device);

    VkPipelineRasterizationStateCreateInfo rs_ci = LvlInitStruct<VkPipelineRasterizationStateCreateInfo>();
    rs_ci.lineWidth = 1.0f;
    rs_ci.rasterizerDiscardEnable = false;

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL_TRY);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_GLSL_TRY);
    vs.InitFromGLSLTry(false, &test_device);
    fs.InitFromGLSLTry(false, &test_device);

    // Set polygonMode=FILL. No error is expected
    {
        VkPipelineObj pipe(&test_device);
        pipe.AddShader(&vs);
        pipe.AddShader(&fs);
        pipe.AddDefaultColorAttachment();
        // Set polygonMode to a good value
        rs_ci.polygonMode = VK_POLYGON_MODE_FILL;
        pipe.SetRasterization(&rs_ci);
        pipe.CreateVKPipeline(pipeline_layout.handle(), render_pass.handle());
    }
}

TEST_F(VkPositiveLayerTest, CreateGraphicsPipelineWithIgnoredPointers) {
    TEST_DESCRIPTION("Create Graphics Pipeline with pointers that must be ignored by layers");
    SetTargetApiVersion(VK_API_VERSION_1_1);
    ASSERT_NO_FATAL_FAILURE(Init());
    if (IsPlatform(kNexusPlayer)) {
        GTEST_SKIP() << "This test should not run on Nexus Player";
    }

    m_depth_stencil_fmt = FindSupportedDepthStencilFormat(gpu());
    m_depthStencil->Init(m_device, m_width, m_height, m_depth_stencil_fmt);

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget(m_depthStencil->BindInfo()));

    const uint64_t fake_address_64 = 0xCDCDCDCDCDCDCDCD;
    const uint64_t fake_address_32 = 0xCDCDCDCD;
    void *hopefully_undereferencable_pointer =
        sizeof(void *) == 8 ? reinterpret_cast<void *>(fake_address_64) : reinterpret_cast<void *>(fake_address_32);

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineShaderStageCreateInfo stages[2] = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};

    const VkPipelineVertexInputStateCreateInfo pipeline_vertex_input_state_create_info{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        nullptr,  // pNext
        0,        // flags
        0,
        nullptr,  // bindings
        0,
        nullptr  // attributes
    };

    const VkPipelineInputAssemblyStateCreateInfo pipeline_input_assembly_state_create_info{
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        nullptr,  // pNext
        0,        // flags
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        VK_FALSE  // primitive restart
    };

    const VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info_template{
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        nullptr,   // pNext
        0,         // flags
        VK_FALSE,  // depthClamp
        VK_FALSE,  // rasterizerDiscardEnable
        VK_POLYGON_MODE_FILL,
        VK_CULL_MODE_NONE,
        VK_FRONT_FACE_COUNTER_CLOCKWISE,
        VK_FALSE,  // depthBias
        0.0f,
        0.0f,
        0.0f,  // depthBias params
        1.0f   // lineWidth
    };

    const VkPipelineMultisampleStateCreateInfo pipeline_multisample_state_create_info{
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        nullptr,  // pNext
        0,        // flags
        VK_SAMPLE_COUNT_1_BIT,
        VK_FALSE,  // sample shading
        0.0f,      // minSampleShading
        nullptr,   // pSampleMask
        VK_FALSE,  // alphaToCoverageEnable
        VK_FALSE   // alphaToOneEnable
    };

    vk_testing::PipelineLayout pipeline_layout;
    {
        VkPipelineLayoutCreateInfo pipeline_layout_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
            0,
            nullptr,  // layouts
            0,
            nullptr  // push constants
        };

        pipeline_layout.init(*m_device, pipeline_layout_create_info, {});
    }

    // try disabled rasterizer and no tessellation
    {
        VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info =
            pipeline_rasterization_state_create_info_template;
        pipeline_rasterization_state_create_info.rasterizerDiscardEnable = VK_TRUE;

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,                              // pNext
            0,                                    // flags
            static_cast<uint32_t>(size(stages)),  // stageCount
            stages,
            &pipeline_vertex_input_state_create_info,
            &pipeline_input_assembly_state_create_info,
            reinterpret_cast<const VkPipelineTessellationStateCreateInfo *>(hopefully_undereferencable_pointer),
            reinterpret_cast<const VkPipelineViewportStateCreateInfo *>(hopefully_undereferencable_pointer),
            &pipeline_rasterization_state_create_info,
            &pipeline_multisample_state_create_info,
            reinterpret_cast<const VkPipelineDepthStencilStateCreateInfo *>(hopefully_undereferencable_pointer),
            reinterpret_cast<const VkPipelineColorBlendStateCreateInfo *>(hopefully_undereferencable_pointer),
            nullptr,  // dynamic states
            pipeline_layout.handle(),
            m_renderPass,
            0,  // subpass
            VK_NULL_HANDLE,
            0};

        vk_testing::Pipeline pipeline(*m_device, graphics_pipeline_create_info);

        m_commandBuffer->begin();
        vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
    }

    // try enabled rasterizer but no subpass attachments
    {
        VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info =
            pipeline_rasterization_state_create_info_template;
        pipeline_rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;

        VkViewport viewport = {0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
        VkRect2D scissor = {{0, 0}, {m_width, m_height}};

        const VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
            1,
            &viewport,
            1,
            &scissor};

        vk_testing::RenderPass render_pass;
        {
            VkSubpassDescription subpass_desc = {};

            VkRenderPassCreateInfo render_pass_create_info{
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                nullptr,  // pNext
                0,        // flags
                0,
                nullptr,  // attachments
                1,
                &subpass_desc,
                0,
                nullptr  // subpass dependencies
            };

            render_pass.init(*m_device, render_pass_create_info);
        }

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{
            VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            nullptr,                              // pNext
            0,                                    // flags
            static_cast<uint32_t>(size(stages)),  // stageCount
            stages,
            &pipeline_vertex_input_state_create_info,
            &pipeline_input_assembly_state_create_info,
            nullptr,
            &pipeline_viewport_state_create_info,
            &pipeline_rasterization_state_create_info,
            &pipeline_multisample_state_create_info,
            reinterpret_cast<const VkPipelineDepthStencilStateCreateInfo *>(hopefully_undereferencable_pointer),
            reinterpret_cast<const VkPipelineColorBlendStateCreateInfo *>(hopefully_undereferencable_pointer),
            nullptr,  // dynamic states
            pipeline_layout.handle(),
            render_pass.handle(),
            0,  // subpass
            VK_NULL_HANDLE,
            0};

        vk_testing::Pipeline pipeline(*m_device, graphics_pipeline_create_info);
    }

    // try dynamic viewport and scissor
    {
        VkPipelineRasterizationStateCreateInfo pipeline_rasterization_state_create_info =
            pipeline_rasterization_state_create_info_template;
        pipeline_rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;

        const VkPipelineViewportStateCreateInfo pipeline_viewport_state_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
            1,
            reinterpret_cast<const VkViewport *>(hopefully_undereferencable_pointer),
            1,
            reinterpret_cast<const VkRect2D *>(hopefully_undereferencable_pointer)};

        const VkPipelineDepthStencilStateCreateInfo pipeline_depth_stencil_state_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
        };

        const VkPipelineColorBlendAttachmentState pipeline_color_blend_attachment_state = {};

        const VkPipelineColorBlendStateCreateInfo pipeline_color_blend_state_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
            VK_FALSE,
            VK_LOGIC_OP_CLEAR,
            1,
            &pipeline_color_blend_attachment_state,
            {0.0f, 0.0f, 0.0f, 0.0f}};

        const VkDynamicState dynamic_states[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        const VkPipelineDynamicStateCreateInfo pipeline_dynamic_state_create_info{
            VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
            nullptr,  // pNext
            0,        // flags
            2, dynamic_states};

        VkGraphicsPipelineCreateInfo graphics_pipeline_create_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                                   nullptr,                              // pNext
                                                                   0,                                    // flags
                                                                   static_cast<uint32_t>(size(stages)),  // stageCount
                                                                   stages,
                                                                   &pipeline_vertex_input_state_create_info,
                                                                   &pipeline_input_assembly_state_create_info,
                                                                   nullptr,
                                                                   &pipeline_viewport_state_create_info,
                                                                   &pipeline_rasterization_state_create_info,
                                                                   &pipeline_multisample_state_create_info,
                                                                   &pipeline_depth_stencil_state_create_info,
                                                                   &pipeline_color_blend_state_create_info,
                                                                   &pipeline_dynamic_state_create_info,  // dynamic states
                                                                   pipeline_layout.handle(),
                                                                   m_renderPass,
                                                                   0,  // subpass
                                                                   VK_NULL_HANDLE,
                                                                   0};

        vk_testing::Pipeline pipeline(*m_device, graphics_pipeline_create_info);
    }
}

TEST_F(VkPositiveLayerTest, CreatePipelineWithCoreChecksDisabled) {
    TEST_DESCRIPTION("Test CreatePipeline while the CoreChecks validation object is disabled");

    // Enable KHR validation features extension
    VkValidationFeatureDisableEXT disables[] = {VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT};
    VkValidationFeaturesEXT features = LvlInitStruct<VkValidationFeaturesEXT>();
    features.disabledValidationFeatureCount = 1;
    features.pDisabledValidationFeatures = disables;

    VkCommandPoolCreateFlags pool_flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ASSERT_NO_FATAL_FAILURE(Init(nullptr, nullptr, pool_flags, &features));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());
    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                 VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_FALSE};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pInputAssemblyState = &iasci;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipeineWithTessellationDomainOrigin) {
    TEST_DESCRIPTION(
        "Test CreatePipeline when VkPipelineTessellationStateCreateInfo.pNext include "
        "VkPipelineTessellationDomainOriginStateCreateInfo");
    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddRequiredExtensions(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    if (!m_device->phy().features().tessellationShader) {
        GTEST_SKIP() << "Device does not support tessellation shaders";
    }

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj tcs(this, bindStateTscShaderText, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    VkShaderObj tes(this, bindStateTeshaderText, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE};

    VkPipelineTessellationDomainOriginStateCreateInfo tessellationDomainOriginStateInfo = {
        VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_DOMAIN_ORIGIN_STATE_CREATE_INFO, VK_NULL_HANDLE,
        VK_TESSELLATION_DOMAIN_ORIGIN_UPPER_LEFT};

    VkPipelineTessellationStateCreateInfo tsci{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
                                               &tessellationDomainOriginStateInfo, 0, 3};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pTessellationState = &tsci;
    pipe.gp_ci_.pInputAssemblyState = &iasci;
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), tcs.GetStageCreateInfo(), tes.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, ViewportArray2NV) {
    TEST_DESCRIPTION("Test to validate VK_NV_viewport_array2");

    AddRequiredExtensions(VK_NV_VIEWPORT_ARRAY_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    VkPhysicalDeviceFeatures available_features = {};
    ASSERT_NO_FATAL_FAILURE(GetPhysicalDeviceFeatures(&available_features));

    if (!available_features.multiViewport) {
        GTEST_SKIP() << "VkPhysicalDeviceFeatures::multiViewport is not supported";
    }
    if (!available_features.tessellationShader) {
        GTEST_SKIP() << "VkPhysicalDeviceFeatures::tessellationShader is not supported";
    }
    if (!available_features.geometryShader) {
        GTEST_SKIP() << "VkPhysicalDeviceFeatures::geometryShader is not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char tcs_src[] = R"glsl(
        #version 450
        layout(vertices = 3) out;

        void main() {
            gl_TessLevelOuter[0] = 4.0f;
            gl_TessLevelOuter[1] = 4.0f;
            gl_TessLevelOuter[2] = 4.0f;
            gl_TessLevelInner[0] = 3.0f;

            gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
        }
    )glsl";

    // Create tessellation control and fragment shader here since they will not be
    // modified by the different test cases.
    VkShaderObj tcs(this, tcs_src, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    const float fp_width = static_cast<float>(m_width);
    const float fp_height = static_cast<float>(m_height);

    std::vector<VkViewport> vps = {{0.0f, 0.0f, fp_width / 2.0f, fp_height}, {fp_width / 2.0f, 0.0f, fp_width / 2.0f, fp_height}};
    std::vector<VkRect2D> scs = {{{0, 0}, {m_width / 2, m_height}},
                                 {{static_cast<int32_t>(m_width) / 2, 0}, {m_width / 2, m_height}}};

    enum class TestStage { VERTEX = 0, TESSELLATION_EVAL = 1, GEOMETRY = 2 };
    std::array<TestStage, 3> vertex_stages = {{TestStage::VERTEX, TestStage::TESSELLATION_EVAL, TestStage::GEOMETRY}};

    // Verify that the usage of gl_ViewportMask[] in the allowed vertex processing
    // stages does not cause any errors.
    for (auto stage : vertex_stages) {
        VkPipelineInputAssemblyStateCreateInfo iaci = LvlInitStruct<VkPipelineInputAssemblyStateCreateInfo>();
        iaci.topology = (stage != TestStage::VERTEX) ? VK_PRIMITIVE_TOPOLOGY_PATCH_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkPipelineTessellationStateCreateInfo tsci = LvlInitStruct<VkPipelineTessellationStateCreateInfo>();
        tsci.patchControlPoints = 3;

        const VkPipelineLayoutObj pl(m_device);

        VkPipelineObj pipe(m_device);
        pipe.AddDefaultColorAttachment();
        pipe.SetInputAssembly(&iaci);
        pipe.SetViewport(vps);
        pipe.SetScissor(scs);
        pipe.AddShader(&fs);

        std::stringstream vs_src, tes_src, geom_src;

        vs_src << R"(
            #version 450
            #extension GL_NV_viewport_array2 : require

            vec2 positions[3] = { vec2( 0.0f, -0.5f),
                                  vec2( 0.5f,  0.5f),
                                  vec2(-0.5f,  0.5f)
                                };
            void main() {)";
        // Write viewportMask if the vertex shader is the last vertex processing stage.
        if (stage == TestStage::VERTEX) {
            vs_src << "gl_ViewportMask[0] = 3;\n";
        }
        vs_src << R"(
                gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);
            })";

        VkShaderObj vs(this, vs_src.str().c_str(), VK_SHADER_STAGE_VERTEX_BIT);
        pipe.AddShader(&vs);

        std::unique_ptr<VkShaderObj> tes, geom;

        if (stage >= TestStage::TESSELLATION_EVAL) {
            tes_src << R"(
                #version 450
                #extension GL_NV_viewport_array2 : require
                layout(triangles) in;

                void main() {
                   gl_Position = (gl_in[0].gl_Position * gl_TessCoord.x +
                                  gl_in[1].gl_Position * gl_TessCoord.y +
                                  gl_in[2].gl_Position * gl_TessCoord.z);)";
            // Write viewportMask if the tess eval shader is the last vertex processing stage.
            if (stage == TestStage::TESSELLATION_EVAL) {
                tes_src << "gl_ViewportMask[0] = 3;\n";
            }
            tes_src << "}";

            tes = std::unique_ptr<VkShaderObj>(
                new VkShaderObj(this, tes_src.str().c_str(), VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT));
            pipe.AddShader(tes.get());
            pipe.AddShader(&tcs);
            pipe.SetTessellation(&tsci);
        }

        if (stage >= TestStage::GEOMETRY) {
            geom_src << R"(
                #version 450
                #extension GL_NV_viewport_array2 : require
                layout(triangles)   in;
                layout(triangle_strip, max_vertices = 3) out;

                void main() {
                   gl_ViewportMask[0] = 3;
                   for(int i = 0; i < 3; ++i) {
                       gl_Position = gl_in[i].gl_Position;
                       EmitVertex();
                    }
                })";

            geom = std::unique_ptr<VkShaderObj>(new VkShaderObj(this, geom_src.str().c_str(), VK_SHADER_STAGE_GEOMETRY_BIT));
            pipe.AddShader(geom.get());
        }

        pipe.CreateVKPipeline(pl.handle(), renderPass());
    }
}

TEST_F(VkPositiveLayerTest, CreatePipelineFragmentOutputNotConsumedButAlphaToCoverageEnabled) {
    TEST_DESCRIPTION(
        "Test that no warning is produced when writing to non-existing color attachment if alpha to coverage is enabled.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget(0u));

    VkPipelineMultisampleStateCreateInfo ms_state_ci = LvlInitStruct<VkPipelineMultisampleStateCreateInfo>();
    ms_state_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    ms_state_ci.alphaToCoverageEnable = VK_TRUE;

    const auto set_info = [&](CreatePipelineHelper &helper) {
        helper.pipe_ms_state_ci_ = ms_state_ci;
        helper.cb_ci_.attachmentCount = 0;
    };
    CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit | kWarningBit);
}

TEST_F(VkPositiveLayerTest, CreatePipelineAttachmentUnused) {
    TEST_DESCRIPTION("Make sure unused attachments are correctly ignored.");

    ASSERT_NO_FATAL_FAILURE(Init());
    if (IsPlatform(kNexusPlayer)) {
        GTEST_SKIP() << "This test should not run on Nexus Player";
    }
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *fsSource = R"glsl(
        #version 450
        layout(location=0) out vec4 x;
        void main(){
           x = vec4(1);  // attachment is unused
        }
    )glsl";
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkAttachmentReference const color_attachments[1]{{VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

    VkSubpassDescription const subpass_descriptions[1]{
        {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, color_attachments, nullptr, nullptr, 0, nullptr}};

    VkAttachmentDescription const attachment_descriptions[1]{{0, VK_FORMAT_B8G8R8A8_UNORM, VK_SAMPLE_COUNT_1_BIT,
                                                              VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
                                                              VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};

    VkRenderPassCreateInfo const render_pass_info{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, attachment_descriptions, 1, subpass_descriptions, 0, nullptr};
    vk_testing::RenderPass render_pass(*m_device, render_pass_info);

    const auto override_info = [&](CreatePipelineHelper &helper) {
        helper.shader_stages_ = {helper.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
        helper.gp_ci_.renderPass = render_pass.handle();
    };
    CreatePipelineHelper::OneshotTest(*this, override_info, kErrorBit | kWarningBit);
}

TEST_F(VkPositiveLayerTest, CreateSurface) {
    TEST_DESCRIPTION("Create and destroy a surface without ever creating a swapchain");

    AddSurfaceExtension();

    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    ASSERT_NO_FATAL_FAILURE(InitState());

    if (!InitSurface()) {
        GTEST_SKIP() << "Cannot create surface";
    }
    DestroySwapchain();  // cleans up both surface and swapchain, if they were created
}

TEST_F(VkPositiveLayerTest, SampleMaskOverrideCoverageNV) {
    TEST_DESCRIPTION("Test to validate VK_NV_sample_mask_override_coverage");

    AddRequiredExtensions(VK_NV_SAMPLE_MASK_OVERRIDE_COVERAGE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());

    const char vs_src[] = R"glsl(
        #version 450
        layout(location=0) out vec4  fragColor;

        const vec2 pos[3] = { vec2( 0.0f, -0.5f),
                              vec2( 0.5f,  0.5f),
                              vec2(-0.5f,  0.5f)
                            };
        void main()
        {
            gl_Position = vec4(pos[gl_VertexIndex % 3], 0.0f, 1.0f);
            fragColor = vec4(0.0f, 1.0f, 0.0f, 1.0f);
        }
    )glsl";

    const char fs_src[] = R"glsl(
        #version 450
        #extension GL_NV_sample_mask_override_coverage : require

        layout(location = 0) in  vec4 fragColor;
        layout(location = 0) out vec4 outColor;

        layout(override_coverage) out int gl_SampleMask[];

        void main()
        {
            gl_SampleMask[0] = 0xff;
            outColor = fragColor;
        }
    )glsl";

    const VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_8_BIT;

    VkAttachmentDescription cAttachment = {};
    cAttachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    cAttachment.samples = sampleCount;
    cAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    cAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    cAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    cAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    cAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    cAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference cAttachRef = {};
    cAttachRef.attachment = 0;
    cAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &cAttachRef;

    VkRenderPassCreateInfo rpci = LvlInitStruct<VkRenderPassCreateInfo>();
    rpci.attachmentCount = 1;
    rpci.pAttachments = &cAttachment;
    rpci.subpassCount = 1;
    rpci.pSubpasses = &subpass;
    vk_testing::RenderPass rp(*m_device, rpci);

    const VkPipelineLayoutObj pl(m_device);

    VkSampleMask sampleMask = 0x01;
    VkPipelineMultisampleStateCreateInfo msaa = LvlInitStruct<VkPipelineMultisampleStateCreateInfo>();
    msaa.rasterizationSamples = sampleCount;
    msaa.sampleShadingEnable = VK_FALSE;
    msaa.pSampleMask = &sampleMask;

    VkPipelineObj pipe(m_device);
    pipe.AddDefaultColorAttachment();
    pipe.SetMSAA(&msaa);

    VkShaderObj vs(this, vs_src, VK_SHADER_STAGE_VERTEX_BIT);
    pipe.AddShader(&vs);

    VkShaderObj fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipe.AddShader(&fs);

    // Create pipeline and make sure that the usage of NV_sample_mask_override_coverage
    // in the fragment shader does not cause any errors.
    pipe.CreateVKPipeline(pl.handle(), rp.handle());
}

TEST_F(VkPositiveLayerTest, TestRasterizationDiscardEnableTrue) {
    TEST_DESCRIPTION("Ensure it doesn't crash and trigger error msg when rasterizerDiscardEnable = true");
    ASSERT_NO_FATAL_FAILURE(Init());
    if (IsPlatform(kNexusPlayer)) {
        GTEST_SKIP() << "This test should not run on Nexus Player";
    }
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    VkAttachmentDescription att[1] = {{}};
    att[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    att[0].samples = VK_SAMPLE_COUNT_4_BIT;
    att[0].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    att[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference cr = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sp = {};
    sp.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sp.colorAttachmentCount = 1;
    sp.pColorAttachments = &cr;
    VkRenderPassCreateInfo rpi = LvlInitStruct<VkRenderPassCreateInfo>();
    rpi.attachmentCount = 1;
    rpi.pAttachments = att;
    rpi.subpassCount = 1;
    rpi.pSubpasses = &sp;
    vk_testing::RenderPass rp(*m_device, rpi);
    ASSERT_TRUE(rp.initialized());

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pViewportState = nullptr;
    pipe.gp_ci_.pMultisampleState = nullptr;
    pipe.gp_ci_.pDepthStencilState = nullptr;
    pipe.gp_ci_.pColorBlendState = nullptr;
    pipe.gp_ci_.renderPass = rp.handle();

    // Skip the test in NexusPlayer. The driver crashes when pViewportState, pMultisampleState, pDepthStencilState, pColorBlendState
    // are NULL.
    pipe.rs_state_ci_.rasterizerDiscardEnable = VK_TRUE;
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, TestSamplerDataForCombinedImageSampler) {
    TEST_DESCRIPTION("Shader code uses sampler data for CombinedImageSampler");
    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fsSource = R"(
                   OpCapability Shader
                   OpMemoryModel Logical GLSL450
                   OpEntryPoint Fragment %main "main"
                   OpExecutionMode %main OriginUpperLeft

                   OpDecorate %InputData DescriptorSet 0
                   OpDecorate %InputData Binding 0
                   OpDecorate %SamplerData DescriptorSet 0
                   OpDecorate %SamplerData Binding 0

               %void = OpTypeVoid
                %f32 = OpTypeFloat 32
              %Image = OpTypeImage %f32 2D 0 0 0 1 Rgba32f
           %ImagePtr = OpTypePointer UniformConstant %Image
          %InputData = OpVariable %ImagePtr UniformConstant
            %Sampler = OpTypeSampler
         %SamplerPtr = OpTypePointer UniformConstant %Sampler
        %SamplerData = OpVariable %SamplerPtr UniformConstant
       %SampledImage = OpTypeSampledImage %Image

               %func = OpTypeFunction %void
               %main = OpFunction %void None %func
                 %40 = OpLabel
           %call_smp = OpLoad %Sampler %SamplerData
                   OpReturn
                   OpFunctionEnd)";

    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.dsl_bindings_ = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr},
    };
    pipe.shader_stages_ = {fs.GetStageCreateInfo(), pipe.vs_->GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();

    VkImageObj image(m_device);
    image.Init(32, 32, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_TILING_OPTIMAL, 0);
    VkImageView view = image.targetView(VK_FORMAT_R8G8B8A8_UNORM);

    vk_testing::Sampler sampler(*m_device, SafeSaneSamplerCreateInfo());

    pipe.descriptor_set_->WriteDescriptorImageInfo(0, view, sampler.handle(), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    pipe.descriptor_set_->UpdateDescriptorSets();

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    vk::CmdBindDescriptorSets(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_layout_.handle(), 0, 1,
                              &pipe.descriptor_set_->set_, 0, NULL);

    vk::CmdDraw(m_commandBuffer->handle(), 3, 1, 0, 0);

    vk::CmdEndRenderPass(m_commandBuffer->handle());
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, NotPointSizeGeometryShaderSuccess) {
    TEST_DESCRIPTION("Create a pipeline using TOPOLOGY_POINT_LIST, but geometry shader doesn't include PointSize.");

    ASSERT_NO_FATAL_FAILURE(Init());

    if ((!m_device->phy().features().geometryShader)) {
        GTEST_SKIP() << "Device does not support the required geometry shader features";
    }
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());
    ASSERT_NO_FATAL_FAILURE(InitViewport());

    VkShaderObj gs(this, bindStateGeomShaderText, VK_SHADER_STAGE_GEOMETRY_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), gs.GetStageCreateInfo(), pipe.fs_->GetStageCreateInfo()};
    pipe.ia_ci_.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, SubpassWithReadOnlyLayoutWithoutDependency) {
    TEST_DESCRIPTION("When both subpasses' attachments are the same and layouts are read-only, they don't need dependency.");
    ASSERT_NO_FATAL_FAILURE(Init());

    auto depth_format = FindSupportedDepthStencilFormat(gpu());

    // A renderpass with one color attachment.
    VkAttachmentDescription attachment = {0,
                                          depth_format,
                                          VK_SAMPLE_COUNT_1_BIT,
                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                          VK_ATTACHMENT_STORE_OP_STORE,
                                          VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                          VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                          VK_IMAGE_LAYOUT_UNDEFINED,
                                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};
    const int size = 2;
    std::array<VkAttachmentDescription, size> attachments = {{attachment, attachment}};

    VkAttachmentReference att_ref_depth_stencil = {0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL};

    std::array<VkSubpassDescription, size> subpasses;
    subpasses[0] = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 0, nullptr, nullptr, &att_ref_depth_stencil, 0, nullptr};
    subpasses[1] = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, 0, 0, nullptr, nullptr, &att_ref_depth_stencil, 0, nullptr};

    VkRenderPassCreateInfo rpci = {
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, size, attachments.data(), size, subpasses.data(), 0, nullptr};
    vk_testing::RenderPass rp(*m_device, rpci);

    // A compatible framebuffer.
    VkImageObj image(m_device);
    image.Init(32, 32, 1, depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_IMAGE_TILING_LINEAR, 0);
    ASSERT_TRUE(image.initialized());

    VkImageViewCreateInfo ivci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                  nullptr,
                                  0,
                                  image.handle(),
                                  VK_IMAGE_VIEW_TYPE_2D,
                                  depth_format,
                                  {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                   VK_COMPONENT_SWIZZLE_IDENTITY},
                                  {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1}};

    vk_testing::ImageView view(*m_device, ivci);
    std::array<VkImageView, size> views = {{view.handle(), view.handle()}};

    VkFramebufferCreateInfo fci = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, rp.handle(), size, views.data(), 32, 32, 1};
    vk_testing::Framebuffer fb(*m_device, fci);

    VkRenderPassBeginInfo rpbi =
        LvlInitStruct<VkRenderPassBeginInfo>(nullptr, rp.handle(), fb.handle(), VkRect2D{{0, 0}, {32u, 32u}}, 0u, nullptr);
    m_commandBuffer->begin();
    vk::CmdBeginRenderPass(m_commandBuffer->handle(), &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdNextSubpass(m_commandBuffer->handle(), VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdEndRenderPass(m_commandBuffer->handle());
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, GeometryShaderPassthroughNV) {
    TEST_DESCRIPTION("Test to validate VK_NV_geometry_shader_passthrough");

    AddRequiredExtensions(VK_NV_GEOMETRY_SHADER_PASSTHROUGH_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    VkPhysicalDeviceFeatures available_features = {};
    ASSERT_NO_FATAL_FAILURE(GetPhysicalDeviceFeatures(&available_features));

    if (!available_features.geometryShader) {
        GTEST_SKIP() << "VkPhysicalDeviceFeatures::geometryShader is not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char vs_src[] = R"glsl(
        #version 450

        out gl_PerVertex {
            vec4 gl_Position;
        };

        layout(location = 0) out ColorBlock {vec4 vertexColor;};

        const vec2 positions[3] = { vec2( 0.0f, -0.5f),
                                    vec2( 0.5f,  0.5f),
                                    vec2(-0.5f,  0.5f)
                                  };

        const vec4 colors[3] = { vec4(1.0f, 0.0f, 0.0f, 1.0f),
                                 vec4(0.0f, 1.0f, 0.0f, 1.0f),
                                 vec4(0.0f, 0.0f, 1.0f, 1.0f)
                               };
        void main()
        {
            vertexColor = colors[gl_VertexIndex % 3];
            gl_Position = vec4(positions[gl_VertexIndex % 3], 0.0, 1.0);
        }
    )glsl";

    const char gs_src[] = R"glsl(
        #version 450
        #extension GL_NV_geometry_shader_passthrough: require

        layout(triangles) in;
        layout(triangle_strip, max_vertices = 3) out;

        layout(passthrough) in gl_PerVertex {vec4 gl_Position;};
        layout(location = 0, passthrough) in ColorBlock {vec4 vertexColor;};

        void main()
        {
           gl_Layer = 0;
        }
    )glsl";

    const char fs_src[] = R"glsl(
        #version 450

        layout(location = 0) in ColorBlock {vec4 vertexColor;};
        layout(location = 0) out vec4 outColor;

        void main() {
            outColor = vertexColor;
        }
    )glsl";

    const VkPipelineLayoutObj pl(m_device);

    VkPipelineObj pipe(m_device);
    pipe.AddDefaultColorAttachment();

    VkShaderObj vs(this, vs_src, VK_SHADER_STAGE_VERTEX_BIT);
    pipe.AddShader(&vs);

    VkShaderObj gs(this, gs_src, VK_SHADER_STAGE_GEOMETRY_BIT);
    pipe.AddShader(&gs);

    VkShaderObj fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipe.AddShader(&fs);

    // Create pipeline and make sure that the usage of NV_geometry_shader_passthrough
    // in the fragment shader does not cause any errors.
    pipe.CreateVKPipeline(pl.handle(), renderPass());
}

TEST_F(VkPositiveLayerTest, PipelineStageConditionalRendering) {
    TEST_DESCRIPTION("Create renderpass and CmdPipelineBarrier with VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_EXT_CONDITIONAL_RENDERING_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto cond_rendering_feature = LvlInitStruct<VkPhysicalDeviceConditionalRenderingFeaturesEXT>();
    auto features2 = GetPhysicalDeviceFeatures2(cond_rendering_feature);
    if (cond_rendering_feature.conditionalRendering == VK_FALSE) {
        GTEST_SKIP() << "conditionalRendering feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // A renderpass with a single subpass that declared a self-dependency
    VkAttachmentDescription attach[] = {
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };
    VkAttachmentReference ref = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpasses[] = {
        {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, &ref, nullptr, nullptr, 0, nullptr},
    };

    VkSubpassDependency dependency = {0,
                                      0,
                                      VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                                      VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT,
                                      VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT,
                                      (VkDependencyFlags)0};
    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, attach, 1, subpasses, 1, &dependency};
    vk_testing::RenderPass rp(*m_device, rpci);

    VkImageObj image(m_device);
    image.Init(32, 32, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    VkImageView imageView = image.targetView(VK_FORMAT_R8G8B8A8_UNORM);

    VkFramebufferCreateInfo fbci = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, rp.handle(), 1, &imageView, 32, 32, 1};
    vk_testing::Framebuffer fb(*m_device, fbci);

    m_commandBuffer->begin();
    VkRenderPassBeginInfo rpbi =
        LvlInitStruct<VkRenderPassBeginInfo>(nullptr, rp.handle(), fb.handle(), VkRect2D{{0, 0}, {32u, 32u}}, 0u, nullptr);
    vk::CmdBeginRenderPass(m_commandBuffer->handle(), &rpbi, VK_SUBPASS_CONTENTS_INLINE);

    VkImageMemoryBarrier imb = LvlInitStruct<VkImageMemoryBarrier>();
    imb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    imb.dstAccessMask = VK_ACCESS_CONDITIONAL_RENDERING_READ_BIT_EXT;
    imb.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imb.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    imb.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imb.image = image.handle();
    imb.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    imb.subresourceRange.baseMipLevel = 0;
    imb.subresourceRange.levelCount = 1;
    imb.subresourceRange.baseArrayLayer = 0;
    imb.subresourceRange.layerCount = 1;

    vk::CmdPipelineBarrier(m_commandBuffer->handle(), VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                           VK_PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT_EXT, 0, 0, nullptr, 0, nullptr, 1, &imb);

    vk::CmdEndRenderPass(m_commandBuffer->handle());
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, CreatePipelineOverlappingPushConstantRange) {
    TEST_DESCRIPTION("Test overlapping push-constant ranges.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *const vsSource = R"glsl(
        #version 450
        layout(push_constant, std430) uniform foo { float x[8]; } constants;
        void main(){
           gl_Position = vec4(constants.x[0]);
        }
    )glsl";

    char const *const fsSource = R"glsl(
        #version 450
        layout(push_constant, std430) uniform foo { float x[4]; } constants;
        layout(location=0) out vec4 o;
        void main(){
           o = vec4(constants.x[0]);
        }
    )glsl";

    VkShaderObj const vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj const fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPushConstantRange push_constant_ranges[2]{{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float) * 8},
                                                {VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float) * 4}};

    VkPipelineLayoutCreateInfo const pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 2, push_constant_ranges};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.pipeline_layout_ci_ = pipeline_layout_info;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, MultipleEntryPointPushConstantVertNormalFrag) {
    TEST_DESCRIPTION("Test push-constant only being used by single entrypoint.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // #version 450
    // layout(push_constant, std430) uniform foo { float x; } consts;
    // void main(){
    //    gl_Position = vec4(consts.x);
    // }
    //
    // #version 450
    // layout(location=0) out vec4 o;
    // void main(){
    //    o = vec4(1.0);
    // }
    const std::string source_body = R"(
                            OpExecutionMode %main_f OriginUpperLeft
                            OpSource GLSL 450
                            OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
                            OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
                            OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
                            OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance
                            OpDecorate %gl_PerVertex Block
                            OpMemberDecorate %foo 0 Offset 0
                            OpDecorate %foo Block
                            OpDecorate %out_frag Location 0
                    %void = OpTypeVoid
                       %3 = OpTypeFunction %void
                   %float = OpTypeFloat 32
                 %v4float = OpTypeVector %float 4
                    %uint = OpTypeInt 32 0
                  %uint_1 = OpConstant %uint 1
       %_arr_float_uint_1 = OpTypeArray %float %uint_1
            %gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
                %out_vert = OpVariable %_ptr_Output_gl_PerVertex Output
                     %int = OpTypeInt 32 1
                   %int_0 = OpConstant %int 0
                     %foo = OpTypeStruct %float
   %_ptr_PushConstant_foo = OpTypePointer PushConstant %foo
                  %consts = OpVariable %_ptr_PushConstant_foo PushConstant
 %_ptr_PushConstant_float = OpTypePointer PushConstant %float
     %_ptr_Output_v4float = OpTypePointer Output %v4float
                %out_frag = OpVariable %_ptr_Output_v4float Output
                 %float_1 = OpConstant %float 1
                 %vec_1_0 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
                  %main_v = OpFunction %void None %3
                 %label_v = OpLabel
                      %20 = OpAccessChain %_ptr_PushConstant_float %consts %int_0
                      %21 = OpLoad %float %20
                      %22 = OpCompositeConstruct %v4float %21 %21 %21 %21
                      %24 = OpAccessChain %_ptr_Output_v4float %out_vert %int_0
                            OpStore %24 %22
                            OpReturn
                            OpFunctionEnd
                  %main_f = OpFunction %void None %3
                 %label_f = OpLabel
                            OpStore %out_frag %vec_1_0
                            OpReturn
                            OpFunctionEnd
    )";

    std::string vert_first = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpEntryPoint Vertex %main_v "main_v" %out_vert
        OpEntryPoint Fragment %main_f "main_f" %out_frag
    )" + source_body;

    std::string frag_first = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpEntryPoint Fragment %main_f "main_f" %out_frag
        OpEntryPoint Vertex %main_v "main_v" %out_vert
    )" + source_body;

    VkPushConstantRange push_constant_ranges[1]{{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(float)}};
    VkPipelineLayoutCreateInfo const pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, push_constant_ranges};

    // Vertex entry point first
    {
        VkShaderObj const vs(this, vert_first.c_str(), VK_SHADER_STAGE_VERTEX_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_v");
        VkShaderObj const fs(this, vert_first.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_f");
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.pipeline_layout_ci_ = pipeline_layout_info;
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // Fragment entry point first
    {
        VkShaderObj const vs(this, frag_first.c_str(), VK_SHADER_STAGE_VERTEX_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_v");
        VkShaderObj const fs(this, frag_first.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_f");
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.pipeline_layout_ci_ = pipeline_layout_info;
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, MultipleEntryPointNormalVertPushConstantFrag) {
    TEST_DESCRIPTION("Test push-constant only being used by single entrypoint.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // #version 450
    // void main(){
    //    gl_Position = vec4(1.0);
    // }
    //
    // #version 450
    // layout(push_constant, std430) uniform foo { float x; } consts;
    // layout(location=0) out vec4 o;
    // void main(){
    //    o = vec4(consts.x);
    // }
    const std::string source_body = R"(
                            OpExecutionMode %main_f OriginUpperLeft
                            OpSource GLSL 450
                            OpMemberDecorate %gl_PerVertex 0 BuiltIn Position
                            OpMemberDecorate %gl_PerVertex 1 BuiltIn PointSize
                            OpMemberDecorate %gl_PerVertex 2 BuiltIn ClipDistance
                            OpMemberDecorate %gl_PerVertex 3 BuiltIn CullDistance
                            OpDecorate %gl_PerVertex Block
                            OpDecorate %out_frag Location 0
                            OpMemberDecorate %foo 0 Offset 0
                            OpDecorate %foo Block
                    %void = OpTypeVoid
                       %3 = OpTypeFunction %void
                   %float = OpTypeFloat 32
                 %v4float = OpTypeVector %float 4
                    %uint = OpTypeInt 32 0
                  %uint_1 = OpConstant %uint 1
       %_arr_float_uint_1 = OpTypeArray %float %uint_1
            %gl_PerVertex = OpTypeStruct %v4float %float %_arr_float_uint_1 %_arr_float_uint_1
%_ptr_Output_gl_PerVertex = OpTypePointer Output %gl_PerVertex
                %out_vert = OpVariable %_ptr_Output_gl_PerVertex Output
                     %int = OpTypeInt 32 1
                   %int_0 = OpConstant %int 0
                 %float_1 = OpConstant %float 1
                      %17 = OpConstantComposite %v4float %float_1 %float_1 %float_1 %float_1
     %_ptr_Output_v4float = OpTypePointer Output %v4float
                %out_frag = OpVariable %_ptr_Output_v4float Output
                     %foo = OpTypeStruct %float
   %_ptr_PushConstant_foo = OpTypePointer PushConstant %foo
                  %consts = OpVariable %_ptr_PushConstant_foo PushConstant
 %_ptr_PushConstant_float = OpTypePointer PushConstant %float
                  %main_v = OpFunction %void None %3
                 %label_v = OpLabel
                      %19 = OpAccessChain %_ptr_Output_v4float %out_vert %int_0
                            OpStore %19 %17
                            OpReturn
                            OpFunctionEnd
                  %main_f = OpFunction %void None %3
                 %label_f = OpLabel
                      %26 = OpAccessChain %_ptr_PushConstant_float %consts %int_0
                      %27 = OpLoad %float %26
                      %28 = OpCompositeConstruct %v4float %27 %27 %27 %27
                            OpStore %out_frag %28
                            OpReturn
                            OpFunctionEnd
    )";

    std::string vert_first = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpEntryPoint Vertex %main_v "main_v" %out_vert
        OpEntryPoint Fragment %main_f "main_f" %out_frag
    )" + source_body;

    std::string frag_first = R"(
        OpCapability Shader
        OpMemoryModel Logical GLSL450
        OpEntryPoint Fragment %main_f "main_f" %out_frag
        OpEntryPoint Vertex %main_v "main_v" %out_vert
    )" + source_body;

    VkPushConstantRange push_constant_ranges[1]{{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)}};
    VkPipelineLayoutCreateInfo const pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, push_constant_ranges};

    // Vertex entry point first
    {
        VkShaderObj const vs(this, vert_first.c_str(), VK_SHADER_STAGE_VERTEX_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_v");
        VkShaderObj const fs(this, vert_first.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_f");
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.pipeline_layout_ci_ = pipeline_layout_info;
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    // Fragment entry point first
    {
        VkShaderObj const vs(this, frag_first.c_str(), VK_SHADER_STAGE_VERTEX_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_v");
        VkShaderObj const fs(this, frag_first.c_str(), VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, nullptr,
                             "main_f");
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            helper.pipeline_layout_ci_ = pipeline_layout_info;
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, PushConstantsCompatibilityGraphicsOnly) {
    TEST_DESCRIPTION("Based on verified valid examples from internal Vulkan Spec issue #2168");
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
    ASSERT_NO_FATAL_FAILURE(InitViewport());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *const vsSource = R"glsl(
        #version 450
        layout(push_constant, std430) uniform foo { float x[16]; } constants;
        void main(){
           gl_Position = vec4(constants.x[4]);
        }
    )glsl";

    VkShaderObj const vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj const fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    // range A and B are the same while range C is different
    const uint32_t pc_size = 32;
    VkPushConstantRange range_a = {VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size};
    VkPushConstantRange range_b = {VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size};
    VkPushConstantRange range_c = {VK_SHADER_STAGE_VERTEX_BIT, 16, pc_size};

    VkPipelineLayoutCreateInfo pipeline_layout_info_a = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &range_a};
    VkPipelineLayoutCreateInfo pipeline_layout_info_b = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &range_b};
    VkPipelineLayoutCreateInfo pipeline_layout_info_c = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &range_c};

    CreatePipelineHelper pipeline_helper_a(*this);  // layout_a and range_a
    CreatePipelineHelper pipeline_helper_b(*this);  // layout_b and range_b
    CreatePipelineHelper pipeline_helper_c(*this);  // layout_c and range_c
    pipeline_helper_a.InitInfo();
    pipeline_helper_a.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipeline_helper_a.pipeline_layout_ci_ = pipeline_layout_info_a;
    pipeline_helper_a.InitState();
    pipeline_helper_a.CreateGraphicsPipeline();
    pipeline_helper_b.InitInfo();
    pipeline_helper_b.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipeline_helper_b.pipeline_layout_ci_ = pipeline_layout_info_b;
    pipeline_helper_b.InitState();
    pipeline_helper_b.CreateGraphicsPipeline();
    pipeline_helper_c.InitInfo();
    pipeline_helper_c.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipeline_helper_c.pipeline_layout_ci_ = pipeline_layout_info_c;
    pipeline_helper_c.InitState();
    pipeline_helper_c.CreateGraphicsPipeline();

    // Easier to see in command buffers
    const VkPipelineLayout layout_a = pipeline_helper_a.pipeline_layout_.handle();
    const VkPipelineLayout layout_b = pipeline_helper_b.pipeline_layout_.handle();
    const VkPipelineLayout layout_c = pipeline_helper_c.pipeline_layout_.handle();
    const VkPipeline pipeline_a = pipeline_helper_a.pipeline_;
    const VkPipeline pipeline_b = pipeline_helper_b.pipeline_;
    const VkPipeline pipeline_c = pipeline_helper_c.pipeline_;

    const float data[16] = {};  // dummy data to match shader size
    const float vbo_data[3] = {1.f, 0.f, 1.f};
    VkConstantBufferObj vbo(m_device, sizeof(vbo_data), (const void *)&vbo_data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // case 1 - bind different layout with the same range
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_b);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 2 - bind layout with same range then push different range
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_b, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_b);
    m_commandBuffer->Draw(1, 0, 0, 0);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 3 - same range same layout then same range from a different layout and same range from the same layout
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_a);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_b, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 4 - same range same layout then diff range and same range update
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_a);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_c, VK_SHADER_STAGE_VERTEX_BIT, 16, pc_size, data);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 5 - update push constant bind different layout with the same range then bind correct layout
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_b);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_a);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 6 - update push constant then bind different layout with overlapping range then bind correct layout
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_c);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_a);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    // case 7 - bind different layout with different range then update push constant and bind correct layout
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_c);
    vk::CmdPushConstants(m_commandBuffer->handle(), layout_a, VK_SHADER_STAGE_VERTEX_BIT, 0, pc_size, data);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_a);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, PushConstantsStaticallyUnused) {
    TEST_DESCRIPTION("Test cases where creating pipeline with no use of push constants but still has ranges in layout");
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));
    ASSERT_NO_FATAL_FAILURE(InitViewport());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // Create set of Pipeline Layouts that cover variations of ranges
    VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_VERTEX_BIT, 0, 4};
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push_constant_range};

    char const *vsSourceUnused = R"glsl(
        #version 450
        layout(push_constant, std430) uniform foo { float x; } consts;
        void main(){
           gl_Position = vec4(1.0);
        }
    )glsl";

    char const *vsSourceEmpty = R"glsl(
        #version 450
        void main(){
           gl_Position = vec4(1.0);
        }
    )glsl";

    VkShaderObj vsUnused(this, vsSourceUnused, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj vsEmpty(this, vsSourceEmpty, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    // Just in layout
    CreatePipelineHelper pipeline_unused(*this);
    pipeline_unused.InitInfo();
    pipeline_unused.shader_stages_ = {vsUnused.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipeline_unused.pipeline_layout_ci_ = pipeline_layout_info;
    pipeline_unused.InitState();
    pipeline_unused.CreateGraphicsPipeline();

    // Shader never had a reference
    CreatePipelineHelper pipeline_empty(*this);
    pipeline_empty.InitInfo();
    pipeline_empty.shader_stages_ = {vsEmpty.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipeline_empty.pipeline_layout_ci_ = pipeline_layout_info;
    pipeline_empty.InitState();
    pipeline_empty.CreateGraphicsPipeline();

    const float vbo_data[3] = {1.f, 0.f, 1.f};
    VkConstantBufferObj vbo(m_device, sizeof(vbo_data), (const void *)&vbo_data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    // Draw without ever pushing to the unused and empty pipelines
    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_unused.pipeline_);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    m_commandBuffer->BindVertexBuffer(&vbo, 0, 1);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_empty.pipeline_);
    m_commandBuffer->Draw(1, 0, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, CreatePipelineSpecializeInt8) {
    TEST_DESCRIPTION("Test int8 specialization.");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto float16int8_features = LvlInitStruct<VkPhysicalDeviceFloat16Int8FeaturesKHR>();
    auto features2 = GetPhysicalDeviceFeatures2(float16int8_features);
    if (float16int8_features.shaderInt8 == VK_FALSE) {
        GTEST_SKIP() << "shaderInt8 feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fs_src = R"(
               OpCapability Shader
               OpCapability Int8
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %v "v"
               OpDecorate %v SpecId 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 8 1
          %v = OpSpecConstant %int 0
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
    )";

    VkShaderObj const fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    const VkSpecializationMapEntry entry = {
        0,               // id
        0,               // offset
        sizeof(uint8_t)  // size
    };
    uint8_t const data = 0x42;
    const VkSpecializationInfo specialization_info = {
        1,
        &entry,
        1 * sizeof(uint8_t),
        &data,
    };

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.shader_stages_[1].pSpecializationInfo = &specialization_info;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineSpecializeInt16) {
    TEST_DESCRIPTION("Test int16 specialization.");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2KHR>();
    GetPhysicalDeviceFeatures2(features2);
    if (features2.features.shaderInt16 == VK_FALSE) {
        GTEST_SKIP() << "shaderInt16 feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fs_src = R"(
               OpCapability Shader
               OpCapability Int16
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %v "v"
               OpDecorate %v SpecId 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 16 1
          %v = OpSpecConstant %int 0
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
    )";

    VkShaderObj const fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    const VkSpecializationMapEntry entry = {
        0,                // id
        0,                // offset
        sizeof(uint16_t)  // size
    };
    uint16_t const data = 0x4342;
    const VkSpecializationInfo specialization_info = {
        1,
        &entry,
        1 * sizeof(uint16_t),
        &data,
    };

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.shader_stages_[1].pSpecializationInfo = &specialization_info;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineSpecializeInt32) {
    TEST_DESCRIPTION("Test int32 specialization.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fs_src = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %v "v"
               OpDecorate %v SpecId 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 32 1
          %v = OpSpecConstant %int 0
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
    )";

    VkShaderObj const fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    const VkSpecializationMapEntry entry = {
        0,                // id
        0,                // offset
        sizeof(uint32_t)  // size
    };
    uint32_t const data = 0x45444342;
    const VkSpecializationInfo specialization_info = {
        1,
        &entry,
        1 * sizeof(uint32_t),
        &data,
    };

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.shader_stages_[1].pSpecializationInfo = &specialization_info;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CreatePipelineSpecializeInt64) {
    TEST_DESCRIPTION("Test int64 specialization.");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2KHR>();
    GetPhysicalDeviceFeatures2(features2);
    if (features2.features.shaderInt64 == VK_FALSE) {
        GTEST_SKIP() << "shaderInt64 feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fs_src = R"(
               OpCapability Shader
               OpCapability Int64
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint Fragment %main "main"
               OpExecutionMode %main OriginUpperLeft
               OpSource GLSL 450
               OpName %main "main"
               OpName %v "v"
               OpDecorate %v SpecId 0
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
        %int = OpTypeInt 64 1
          %v = OpSpecConstant %int 0
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
    )";

    VkShaderObj const fs(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM);

    const VkSpecializationMapEntry entry = {
        0,                // id
        0,                // offset
        sizeof(uint64_t)  // size
    };
    uint64_t const data = 0x4948474645444342;
    const VkSpecializationInfo specialization_info = {
        1,
        &entry,
        1 * sizeof(uint64_t),
        &data,
    };

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.shader_stages_[1].pSpecializationInfo = &specialization_info;
    pipe.InitState();

    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, SeparateDepthStencilSubresourceLayout) {
    TEST_DESCRIPTION("Test that separate depth stencil layouts are tracked correctly.");
    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddRequiredExtensions(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SEPARATE_DEPTH_STENCIL_LAYOUTS_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << VK_EXT_COLOR_WRITE_ENABLE_EXTENSION_NAME << " not supported";
    }

    auto separate_features = LvlInitStruct<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures>();
    auto features2 = GetPhysicalDeviceFeatures2(separate_features);
    if (!separate_features.separateDepthStencilLayouts) {
        printf("separateDepthStencilLayouts feature not supported, skipping tests\n");
        return;
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));

    VkFormat ds_format = VK_FORMAT_D24_UNORM_S8_UINT;
    VkFormatProperties props;
    vk::GetPhysicalDeviceFormatProperties(gpu(), ds_format, &props);
    if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0) {
        ds_format = VK_FORMAT_D32_SFLOAT_S8_UINT;
        vk::GetPhysicalDeviceFormatProperties(gpu(), ds_format, &props);
        ASSERT_TRUE((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0);
    }

    auto image_ci = vk_testing::Image::create_info();
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.extent.width = 64;
    image_ci.extent.height = 64;
    image_ci.mipLevels = 1;
    image_ci.arrayLayers = 6;
    image_ci.format = ds_format;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    vk_testing::Image image;
    image.init(*m_device, image_ci);

    const auto depth_range = image.subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT);
    const auto stencil_range = image.subresource_range(VK_IMAGE_ASPECT_STENCIL_BIT);
    const auto depth_stencil_range = image.subresource_range(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

    vk_testing::ImageView view;
    VkImageViewCreateInfo view_info = LvlInitStruct<VkImageViewCreateInfo>();
    view_info.image = image.handle();
    view_info.subresourceRange = depth_stencil_range;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    view_info.format = ds_format;
    view.init(*m_device, view_info);

    std::vector<VkImageMemoryBarrier> barriers;

    {
        m_commandBuffer->begin();
        auto depth_barrier =
            image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depth_range);
        auto stencil_barrier =
            image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL, stencil_range);
        vk::CmdPipelineBarrier(m_commandBuffer->handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &depth_barrier);
        vk::CmdPipelineBarrier(m_commandBuffer->handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &stencil_barrier);
        m_commandBuffer->end();
        m_commandBuffer->QueueCommandBuffer(false);
        m_commandBuffer->reset();
    }

    m_commandBuffer->begin();

    // Test that we handle initial layout in command buffer.
    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depth_stencil_range));

    // Test that we can transition aspects separately and use specific layouts.
    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL, depth_range));

    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL, stencil_range));

    // Test that transition from UNDEFINED on depth aspect does not clobber stencil layout.
    barriers.push_back(
        image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, depth_range));

    // Test that we can transition aspects separately and use combined layouts. (Only care about the aspect in question).
    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, depth_range));

    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, stencil_range));

    // Test that we can transition back again with combined layout.
    barriers.push_back(image.image_memory_barrier(0, 0, VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL,
                                                  VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, depth_stencil_range));

    VkRenderPassBeginInfo rp_begin_info = LvlInitStruct<VkRenderPassBeginInfo>();
    VkRenderPassCreateInfo2 rp2 = LvlInitStruct<VkRenderPassCreateInfo2>();
    VkAttachmentDescription2 desc = LvlInitStruct<VkAttachmentDescription2>();
    VkSubpassDescription2 sub = LvlInitStruct<VkSubpassDescription2>();
    VkAttachmentReference2 att = LvlInitStruct<VkAttachmentReference2>();
    VkAttachmentDescriptionStencilLayout stencil_desc = LvlInitStruct<VkAttachmentDescriptionStencilLayout>();
    VkAttachmentReferenceStencilLayout stencil_att = LvlInitStruct<VkAttachmentReferenceStencilLayout>();
    // Test that we can discard stencil layout.
    stencil_desc.stencilInitialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    stencil_desc.stencilFinalLayout = VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL;
    stencil_att.stencilLayout = VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL;

    desc.format = ds_format;
    desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    desc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    desc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    desc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
    desc.samples = VK_SAMPLE_COUNT_1_BIT;
    desc.pNext = &stencil_desc;

    att.layout = VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL;
    att.attachment = 0;
    att.pNext = &stencil_att;

    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.pDepthStencilAttachment = &att;
    rp2.subpassCount = 1;
    rp2.pSubpasses = &sub;
    rp2.attachmentCount = 1;
    rp2.pAttachments = &desc;
    vk_testing::RenderPass render_pass_separate(*m_device, rp2, true);

    desc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    desc.finalLayout = desc.initialLayout;
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    desc.pNext = nullptr;
    att.layout = desc.initialLayout;
    att.pNext = nullptr;
    vk_testing::RenderPass render_pass_combined(*m_device, rp2, true);

    VkFramebufferCreateInfo fb_info = LvlInitStruct<VkFramebufferCreateInfo>();
    fb_info.renderPass = render_pass_separate.handle();
    fb_info.width = 1;
    fb_info.height = 1;
    fb_info.layers = 1;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &view.handle();
    vk_testing::Framebuffer framebuffer_separate(*m_device, fb_info);

    fb_info.renderPass = render_pass_combined.handle();
    vk_testing::Framebuffer framebuffer_combined(*m_device, fb_info);

    for (auto &barrier : barriers) {
        vk::CmdPipelineBarrier(m_commandBuffer->handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
                               0, nullptr, 0, nullptr, 1, &barrier);
    }

    rp_begin_info.renderPass = render_pass_separate.handle();
    rp_begin_info.framebuffer = framebuffer_separate.handle();
    rp_begin_info.renderArea.extent = {1, 1};
    vk::CmdBeginRenderPass(m_commandBuffer->handle(), &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdEndRenderPass(m_commandBuffer->handle());

    rp_begin_info.renderPass = render_pass_combined.handle();
    rp_begin_info.framebuffer = framebuffer_combined.handle();
    vk::CmdBeginRenderPass(m_commandBuffer->handle(), &rp_begin_info, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdEndRenderPass(m_commandBuffer->handle());

    m_commandBuffer->end();
    m_commandBuffer->QueueCommandBuffer(false);
}

TEST_F(VkPositiveLayerTest, SwapchainImageFormatProps) {
    TEST_DESCRIPTION("Try using special format props on a swapchain image");

    AddSurfaceExtension();
    ASSERT_NO_FATAL_FAILURE(InitFramework());

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());

    if (!InitSwapchain()) {
        GTEST_SKIP() << "Cannot create surface or swapchain";
    }

    // HACK: I know InitSwapchain() will pick first supported format
    VkSurfaceFormatKHR format_tmp;
    {
        uint32_t format_count = 1;
        const VkResult err = vk::GetPhysicalDeviceSurfaceFormatsKHR(gpu(), m_surface, &format_count, &format_tmp);
        ASSERT_TRUE(err == VK_SUCCESS || err == VK_INCOMPLETE) << vk_result_string(err);
    }
    const VkFormat format = format_tmp.format;

    VkFormatProperties format_props;
    vk::GetPhysicalDeviceFormatProperties(gpu(), format, &format_props);
    if (!(format_props.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)) {
        GTEST_SKIP() << "We need VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT feature";
    }

    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineLayoutObj pipeline_layout(DeviceObj());
    VkRenderpassObj render_pass(DeviceObj(), format);

    VkPipelineObj pipeline(DeviceObj());
    pipeline.AddShader(&vs);
    pipeline.AddShader(&fs);
    VkPipelineColorBlendAttachmentState pcbas = {};
    pcbas.blendEnable = VK_TRUE;  // !!!
    pcbas.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    pipeline.AddColorAttachment(0, pcbas);
    pipeline.MakeDynamic(VK_DYNAMIC_STATE_VIEWPORT);
    pipeline.MakeDynamic(VK_DYNAMIC_STATE_SCISSOR);

    ASSERT_VK_SUCCESS(pipeline.CreateVKPipeline(pipeline_layout.handle(), render_pass.handle()));

    uint32_t image_count;
    ASSERT_VK_SUCCESS(vk::GetSwapchainImagesKHR(device(), m_swapchain, &image_count, nullptr));
    std::vector<VkImage> swapchain_images(image_count);
    ASSERT_VK_SUCCESS(vk::GetSwapchainImagesKHR(device(), m_swapchain, &image_count, swapchain_images.data()));

    VkFenceObj fence;
    fence.init(*DeviceObj(), VkFenceObj::create_info());

    uint32_t image_index;
    ASSERT_VK_SUCCESS(vk::AcquireNextImageKHR(device(), m_swapchain, kWaitTimeout, VK_NULL_HANDLE, fence.handle(), &image_index));
    fence.wait(vvl::kU32Max);

    VkImageViewCreateInfo ivci = LvlInitStruct<VkImageViewCreateInfo>();
    ivci.image = swapchain_images[image_index];
    ivci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivci.format = format;
    ivci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk_testing::ImageView image_view(*m_device, ivci);

    VkFramebufferCreateInfo fbci = LvlInitStruct<VkFramebufferCreateInfo>();
    fbci.renderPass = render_pass.handle();
    fbci.attachmentCount = 1;
    fbci.pAttachments = &image_view.handle();
    fbci.width = 1;
    fbci.height = 1;
    fbci.layers = 1;
    vk_testing::Framebuffer framebuffer(*m_device, fbci);

    VkCommandBufferObj cmdbuff(DeviceObj(), m_commandPool);
    cmdbuff.begin();
    VkRenderPassBeginInfo rpbi = LvlInitStruct<VkRenderPassBeginInfo>();
    rpbi.renderPass = render_pass.handle();
    rpbi.framebuffer = framebuffer.handle();
    rpbi.renderArea = {{0, 0}, {1, 1}};
    cmdbuff.BeginRenderPass(rpbi);

    vk::CmdBindPipeline(cmdbuff.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle());
}

TEST_F(VkPositiveLayerTest, SwapchainExclusiveModeQueueFamilyPropertiesReferences) {
    TEST_DESCRIPTION("Try using special format props on a swapchain image");

    AddSurfaceExtension();

    ASSERT_NO_FATAL_FAILURE(InitFramework());

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());
    if (!InitSurface()) {
        GTEST_SKIP() << "Cannot create surface";
    }
    InitSwapchainInfo();

    VkBool32 supported;
    vk::GetPhysicalDeviceSurfaceSupportKHR(gpu(), m_device->graphics_queue_node_index_, m_surface, &supported);
    if (!supported) {
        GTEST_SKIP() << "Graphics queue does not support present";
    }

    auto surface = m_surface;
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkSwapchainCreateInfoKHR swapchain_create_info = LvlInitStruct<VkSwapchainCreateInfoKHR>();
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = m_surface_capabilities.minImageCount;
    swapchain_create_info.imageFormat = m_surface_formats[0].format;
    swapchain_create_info.imageColorSpace = m_surface_formats[0].colorSpace;
    swapchain_create_info.imageExtent = {m_surface_capabilities.minImageExtent.width, m_surface_capabilities.minImageExtent.height};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = imageUsage;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = preTransform;
    swapchain_create_info.compositeAlpha = m_surface_composite_alpha;
    swapchain_create_info.presentMode = m_surface_non_shared_present_mode;
    swapchain_create_info.clipped = VK_FALSE;
    swapchain_create_info.oldSwapchain = 0;

    swapchain_create_info.queueFamilyIndexCount = 4094967295;  // This SHOULD get ignored
    uint32_t bogus_int = 99;
    swapchain_create_info.pQueueFamilyIndices = &bogus_int;

    vk::CreateSwapchainKHR(device(), &swapchain_create_info, nullptr, &m_swapchain);

    // Create another device, create another swapchain, and use this one for oldSwapchain
    // It is legal to include an 'oldSwapchain' object that is from a different device
    const float q_priority[] = {1.0f};
    VkDeviceQueueCreateInfo queue_ci = LvlInitStruct<VkDeviceQueueCreateInfo>();
    queue_ci.queueFamilyIndex = 0;
    queue_ci.queueCount = 1;
    queue_ci.pQueuePriorities = q_priority;

    VkDeviceCreateInfo device_ci = LvlInitStruct<VkDeviceCreateInfo>();
    device_ci.queueCreateInfoCount = 1;
    device_ci.pQueueCreateInfos = &queue_ci;
    device_ci.ppEnabledExtensionNames = m_device_extension_names.data();
    device_ci.enabledExtensionCount = m_device_extension_names.size();

    VkDevice test_device;
    vk::CreateDevice(gpu(), &device_ci, nullptr, &test_device);

    swapchain_create_info.oldSwapchain = m_swapchain;
    VkSwapchainKHR new_swapchain = VK_NULL_HANDLE;
    vk::CreateSwapchainKHR(test_device, &swapchain_create_info, nullptr, &new_swapchain);

    if (new_swapchain != VK_NULL_HANDLE) {
        vk::DestroySwapchainKHR(test_device, new_swapchain, nullptr);
    }

    vk::DestroyDevice(test_device, nullptr);

    if (m_surface != VK_NULL_HANDLE) {
        vk::DestroySurfaceKHR(instance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

TEST_F(VkPositiveLayerTest, ProtectedAndUnprotectedQueue) {
    TEST_DESCRIPTION("Test creating 2 queues, 1 protected, and getting both with vkGetDeviceQueue2");
    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    // NOTE (ncesario): This appears to be failing in the driver on the Shield.
    //      It's clear what is causing this; more investigation is necessary.
    if (IsPlatform(kShieldTV) || IsPlatform(kShieldTVb)) {
        GTEST_SKIP() << "Test not supported by Shield TV";
    }

    // Needed for both protected memory and vkGetDeviceQueue2
    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    auto protected_features = LvlInitStruct<VkPhysicalDeviceProtectedMemoryFeatures>();
    GetPhysicalDeviceFeatures2(protected_features);
    if (protected_features.protectedMemory == VK_FALSE) {
        GTEST_SKIP() << "test requires protectedMemory";
    }

    // Try to find a protected queue family type
    bool protected_queue = false;
    VkQueueFamilyProperties queue_properties;  // selected queue family used
    uint32_t queue_family_index = 0;
    uint32_t queue_family_count = 0;
    vk::GetPhysicalDeviceQueueFamilyProperties(gpu(), &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vk::GetPhysicalDeviceQueueFamilyProperties(gpu(), &queue_family_count, queue_families.data());

    for (size_t i = 0; i < queue_families.size(); i++) {
        // need to have at least 2 queues to use
        if (((queue_families[i].queueFlags & VK_QUEUE_PROTECTED_BIT) != 0) && (queue_families[i].queueCount > 1)) {
            protected_queue = true;
            queue_family_index = i;
            queue_properties = queue_families[i];
            break;
        }
    }

    if (protected_queue == false) {
        GTEST_SKIP() << "test requires queue family with VK_QUEUE_PROTECTED_BIT and 2 queues, not available.";
    }

    float queue_priority = 1.0;

    VkDeviceQueueCreateInfo queue_create_info[2];
    queue_create_info[0] = LvlInitStruct<VkDeviceQueueCreateInfo>();
    queue_create_info[0].flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    queue_create_info[0].queueFamilyIndex = queue_family_index;
    queue_create_info[0].queueCount = 1;
    queue_create_info[0].pQueuePriorities = &queue_priority;

    queue_create_info[1] = LvlInitStruct<VkDeviceQueueCreateInfo>();
    queue_create_info[1].flags = 0;  // unprotected because the protected flag is not set
    queue_create_info[1].queueFamilyIndex = queue_family_index;
    queue_create_info[1].queueCount = 1;
    queue_create_info[1].pQueuePriorities = &queue_priority;

    VkDevice test_device = VK_NULL_HANDLE;
    VkDeviceCreateInfo device_create_info = LvlInitStruct<VkDeviceCreateInfo>(&protected_features);
    device_create_info.flags = 0;
    device_create_info.pQueueCreateInfos = queue_create_info;
    device_create_info.queueCreateInfoCount = 2;
    device_create_info.pEnabledFeatures = nullptr;
    device_create_info.enabledLayerCount = 0;
    device_create_info.enabledExtensionCount = 0;
    ASSERT_VK_SUCCESS(vk::CreateDevice(gpu(), &device_create_info, nullptr, &test_device));

    VkQueue test_queue_protected = VK_NULL_HANDLE;
    VkQueue test_queue_unprotected = VK_NULL_HANDLE;

    PFN_vkGetDeviceQueue2 vkGetDeviceQueue2 = (PFN_vkGetDeviceQueue2)vk::GetDeviceProcAddr(test_device, "vkGetDeviceQueue2");
    ASSERT_TRUE(vkGetDeviceQueue2 != nullptr);

    VkDeviceQueueInfo2 queue_info_2 = LvlInitStruct<VkDeviceQueueInfo2>();

    queue_info_2.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    queue_info_2.queueFamilyIndex = queue_family_index;
    queue_info_2.queueIndex = 0;
    vkGetDeviceQueue2(test_device, &queue_info_2, &test_queue_protected);

    queue_info_2.flags = 0;
    queue_info_2.queueIndex = 0;
    vkGetDeviceQueue2(test_device, &queue_info_2, &test_queue_unprotected);

    vk::DestroyDevice(test_device, nullptr);
}

TEST_F(VkPositiveLayerTest, ShaderFloatControl) {
    TEST_DESCRIPTION("Test VK_KHR_float_controls");

    // Need 1.1 to get SPIR-V 1.3 since OpExecutionModeId was added in SPIR-V 1.2
    SetTargetApiVersion(VK_API_VERSION_1_1);
    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    PFN_vkGetPhysicalDeviceProperties2KHR vkGetPhysicalDeviceProperties2KHR =
        (PFN_vkGetPhysicalDeviceProperties2KHR)vk::GetInstanceProcAddr(instance(), "vkGetPhysicalDeviceProperties2KHR");
    ASSERT_TRUE(vkGetPhysicalDeviceProperties2KHR != nullptr);

    auto shader_float_control = LvlInitStruct<VkPhysicalDeviceFloatControlsProperties>();
    auto properties2 = LvlInitStruct<VkPhysicalDeviceProperties2KHR>(&shader_float_control);
    vkGetPhysicalDeviceProperties2KHR(gpu(), &properties2);

    bool signed_zero_inf_nan_preserve = (shader_float_control.shaderSignedZeroInfNanPreserveFloat32 == VK_TRUE);
    bool denorm_preserve = (shader_float_control.shaderDenormPreserveFloat32 == VK_TRUE);
    bool denorm_flush_to_zero = (shader_float_control.shaderDenormFlushToZeroFloat32 == VK_TRUE);
    bool rounding_mode_rte = (shader_float_control.shaderRoundingModeRTEFloat32 == VK_TRUE);
    bool rounding_mode_rtz = (shader_float_control.shaderRoundingModeRTZFloat32 == VK_TRUE);

    // same body for each shader, only the start is different
    // this is just "float a = 1.0 + 2.0;" in SPIR-V
    const std::string source_body = R"(
             OpExecutionMode %main LocalSize 1 1 1
             OpSource GLSL 450
             OpName %main "main"
     %void = OpTypeVoid
        %3 = OpTypeFunction %void
    %float = OpTypeFloat 32
%pFunction = OpTypePointer Function %float
  %float_3 = OpConstant %float 3
     %main = OpFunction %void None %3
        %5 = OpLabel
        %6 = OpVariable %pFunction Function
             OpStore %6 %float_3
             OpReturn
             OpFunctionEnd
)";

    if (signed_zero_inf_nan_preserve) {
        const std::string spv_source = R"(
            OpCapability Shader
            OpCapability SignedZeroInfNanPreserve
            OpExtension "SPV_KHR_float_controls"
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint GLCompute %main "main"
            OpExecutionMode %main SignedZeroInfNanPreserve 32
)" + source_body;

        const auto set_info = [&](CreateComputePipelineHelper &helper) {
            helper.cs_.reset(
                new VkShaderObj(this, spv_source.c_str(), VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1, SPV_SOURCE_ASM));
        };
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    if (denorm_preserve) {
        const std::string spv_source = R"(
            OpCapability Shader
            OpCapability DenormPreserve
            OpExtension "SPV_KHR_float_controls"
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint GLCompute %main "main"
            OpExecutionMode %main DenormPreserve 32
)" + source_body;

        const auto set_info = [&](CreateComputePipelineHelper &helper) {
            helper.cs_.reset(
                new VkShaderObj(this, spv_source.c_str(), VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1, SPV_SOURCE_ASM));
        };
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    if (denorm_flush_to_zero) {
        const std::string spv_source = R"(
            OpCapability Shader
            OpCapability DenormFlushToZero
            OpExtension "SPV_KHR_float_controls"
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint GLCompute %main "main"
            OpExecutionMode %main DenormFlushToZero 32
)" + source_body;

        const auto set_info = [&](CreateComputePipelineHelper &helper) {
            helper.cs_.reset(
                new VkShaderObj(this, spv_source.c_str(), VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1, SPV_SOURCE_ASM));
        };
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    if (rounding_mode_rte) {
        const std::string spv_source = R"(
            OpCapability Shader
            OpCapability RoundingModeRTE
            OpExtension "SPV_KHR_float_controls"
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint GLCompute %main "main"
            OpExecutionMode %main RoundingModeRTE 32
)" + source_body;

        const auto set_info = [&](CreateComputePipelineHelper &helper) {
            helper.cs_.reset(
                new VkShaderObj(this, spv_source.c_str(), VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1, SPV_SOURCE_ASM));
        };
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    if (rounding_mode_rtz) {
        const std::string spv_source = R"(
            OpCapability Shader
            OpCapability RoundingModeRTZ
            OpExtension "SPV_KHR_float_controls"
       %1 = OpExtInstImport "GLSL.std.450"
            OpMemoryModel Logical GLSL450
            OpEntryPoint GLCompute %main "main"
            OpExecutionMode %main RoundingModeRTZ 32
)" + source_body;

        const auto set_info = [&](CreateComputePipelineHelper &helper) {
            helper.cs_.reset(
                new VkShaderObj(this, spv_source.c_str(), VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1, SPV_SOURCE_ASM));
        };
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, Storage8and16bit) {
    TEST_DESCRIPTION("Test VK_KHR_8bit_storage and VK_KHR_16bit_storage");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_STORAGE_BUFFER_STORAGE_CLASS_EXTENSION_NAME);
    AddOptionalExtensions(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    AddOptionalExtensions(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    bool support_8_bit = IsExtensionsEnabled(VK_KHR_8BIT_STORAGE_EXTENSION_NAME);
    bool support_16_bit = IsExtensionsEnabled(VK_KHR_16BIT_STORAGE_EXTENSION_NAME);

    if ((support_8_bit == false) && (support_16_bit == false)) {
        GTEST_SKIP() << "Extension not supported";
    }

    auto storage_8_bit_features = LvlInitStruct<VkPhysicalDevice8BitStorageFeaturesKHR>();
    auto storage_16_bit_features = LvlInitStruct<VkPhysicalDevice16BitStorageFeaturesKHR>(&storage_8_bit_features);
    auto float_16_int_8_features = LvlInitStruct<VkPhysicalDeviceShaderFloat16Int8Features>(&storage_16_bit_features);
    auto features2 = GetPhysicalDeviceFeatures2(float_16_int_8_features);
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // 8 bit int test (not 8 bit float support in Vulkan)
    if ((support_8_bit == true) && (float_16_int_8_features.shaderInt8 == VK_TRUE)) {
        if (storage_8_bit_features.storageBuffer8BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_8bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
                layout(set = 0, binding = 0) buffer SSBO { int8_t x; } data;
                void main(){
                   int8_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_8_bit_features.uniformAndStorageBuffer8BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_8bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
                layout(set = 0, binding = 0) uniform UBO { int8_t x; } data;
                void main(){
                   int8_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_8_bit_features.storagePushConstant8 == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_8bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int8: enable
                layout(push_constant) uniform PushConstant { int8_t x; } data;
                void main(){
                   int8_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_VERTEX_BIT, 0, 4};
            VkPipelineLayoutCreateInfo pipeline_layout_info{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push_constant_range};
            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.pipeline_layout_ci_ = pipeline_layout_info;
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }
    }

    // 16 bit float tests
    if ((support_16_bit == true) && (float_16_int_8_features.shaderFloat16 == VK_TRUE)) {
        if (storage_16_bit_features.storageBuffer16BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
                layout(set = 0, binding = 0) buffer SSBO { float16_t x; } data;
                void main(){
                   float16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.uniformAndStorageBuffer16BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
                layout(set = 0, binding = 0) uniform UBO { float16_t x; } data;
                void main(){
                   float16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.storagePushConstant16 == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
                layout(push_constant) uniform PushConstant { float16_t x; } data;
                void main(){
                   float16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_VERTEX_BIT, 0, 4};
            VkPipelineLayoutCreateInfo pipeline_layout_info{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push_constant_range};
            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.pipeline_layout_ci_ = pipeline_layout_info;
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.storageInputOutput16 == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
                layout(location = 0) out float16_t outData;
                void main(){
                   outData = float16_t(1);
                   gl_Position = vec4(0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            // Need to match in/out
            char const *fsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_float16: enable
                layout(location = 0) in float16_t x;
                layout(location = 0) out vec4 uFragColor;
                void main(){
                   uFragColor = vec4(0,1,0,1);
                }
            )glsl";
            VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }
    }

    // 16 bit int tests
    if ((support_16_bit == true) && (features2.features.shaderInt16 == VK_TRUE)) {
        if (storage_16_bit_features.storageBuffer16BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int16: enable
                layout(set = 0, binding = 0) buffer SSBO { int16_t x; } data;
                void main(){
                   int16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.uniformAndStorageBuffer16BitAccess == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int16: enable
                layout(set = 0, binding = 0) uniform UBO { int16_t x; } data;
                void main(){
                   int16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.storagePushConstant16 == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int16: enable
                layout(push_constant) uniform PushConstant { int16_t x; } data;
                void main(){
                   int16_t a = data.x + data.x;
                   gl_Position = vec4(float(a) * 0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            VkPushConstantRange push_constant_range = {VK_SHADER_STAGE_VERTEX_BIT, 0, 4};
            VkPipelineLayoutCreateInfo pipeline_layout_info{
                VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 0, nullptr, 1, &push_constant_range};
            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
                helper.pipeline_layout_ci_ = pipeline_layout_info;
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }

        if (storage_16_bit_features.storageInputOutput16 == VK_TRUE) {
            char const *vsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int16: enable
                layout(location = 0) out int16_t outData;
                void main(){
                   outData = int16_t(1);
                   gl_Position = vec4(0.0);
                }
            )glsl";
            VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);

            // Need to match in/out
            char const *fsSource = R"glsl(
                #version 450
                #extension GL_EXT_shader_16bit_storage: enable
                #extension GL_EXT_shader_explicit_arithmetic_types_int16: enable
                layout(location = 0) flat in int16_t x;
                layout(location = 0) out vec4 uFragColor;
                void main(){
                   uFragColor = vec4(0,1,0,1);
                }
            )glsl";
            VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

            const auto set_info = [&](CreatePipelineHelper &helper) {
                helper.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
            };
            CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
        }
    }
}

TEST_F(VkPositiveLayerTest, ReadShaderClock) {
    TEST_DESCRIPTION("Test VK_KHR_shader_clock");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_CLOCK_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto shader_clock_features = LvlInitStruct<VkPhysicalDeviceShaderClockFeaturesKHR>();
    auto features2 = GetPhysicalDeviceFeatures2(shader_clock_features);
    if ((shader_clock_features.shaderDeviceClock == VK_FALSE) && (shader_clock_features.shaderSubgroupClock == VK_FALSE)) {
        // shaderSubgroupClock should be supported, but extra check
        GTEST_SKIP() << "no support for shaderDeviceClock or shaderSubgroupClock";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // Device scope using GL_EXT_shader_realtime_clock
    char const *vsSourceDevice = R"glsl(
        #version 450
        #extension GL_EXT_shader_realtime_clock: enable
        void main(){
           uvec2 a = clockRealtime2x32EXT();
           gl_Position = vec4(float(a.x) * 0.0);
        }
    )glsl";
    VkShaderObj vs_device(this, vsSourceDevice, VK_SHADER_STAGE_VERTEX_BIT);

    // Subgroup scope using ARB_shader_clock
    char const *vsSourceScope = R"glsl(
        #version 450
        #extension GL_ARB_shader_clock: enable
        void main(){
           uvec2 a = clock2x32ARB();
           gl_Position = vec4(float(a.x) * 0.0);
        }
    )glsl";
    VkShaderObj vs_subgroup(this, vsSourceScope, VK_SHADER_STAGE_VERTEX_BIT);

    if (shader_clock_features.shaderDeviceClock == VK_TRUE) {
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs_device.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }

    if (shader_clock_features.shaderSubgroupClock == VK_TRUE) {
        const auto set_info = [&](CreatePipelineHelper &helper) {
            helper.shader_stages_ = {vs_subgroup.GetStageCreateInfo(), helper.fs_->GetStageCreateInfo()};
        };
        CreatePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, PhysicalStorageBuffer) {
    TEST_DESCRIPTION("Reproduces Github issue #2467 and effectively #2465 as well.");

    SetTargetApiVersion(VK_API_VERSION_1_2);
    AddRequiredExtensions(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (DeviceValidationVersion() < VK_API_VERSION_1_2) {
        GTEST_SKIP() << "At least Vulkan version 1.2 is required";
    }

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto features12 = LvlInitStruct<VkPhysicalDeviceVulkan12Features>();
    auto features2 = GetPhysicalDeviceFeatures2(features12);
    if (VK_TRUE != features12.bufferDeviceAddress) {
        GTEST_SKIP() << "VkPhysicalDeviceVulkan12Features::bufferDeviceAddress not supported and is required";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *vertex_source = R"glsl(
#version 450

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable

layout(buffer_reference, buffer_reference_align=16, scalar) readonly buffer VectorBuffer {
  vec3 v;
};

layout(push_constant, scalar) uniform pc {
  VectorBuffer vb;
} pcs;

void main() {
    gl_Position = vec4(pcs.vb.v, 1.0);
}
        )glsl";
    const VkShaderObj vs(this, vertex_source, VK_SHADER_STAGE_VERTEX_BIT);

    const char *fragment_source = R"glsl(
#version 450

#extension GL_EXT_buffer_reference : enable
#extension GL_EXT_scalar_block_layout : enable

layout(buffer_reference, buffer_reference_align=16, scalar) readonly buffer VectorBuffer {
  vec3 v;
};

layout(push_constant, scalar) uniform pushConstants {
  layout(offset=8) VectorBuffer vb;
} pcs;

layout(location=0) out vec4 o;
void main() {
    o = vec4(pcs.vb.v, 1.0);
}
    )glsl";
    const VkShaderObj fs(this, fragment_source, VK_SHADER_STAGE_FRAGMENT_BIT);

    std::array<VkPushConstantRange, 2> push_ranges;
    push_ranges[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_ranges[0].size = sizeof(uint64_t);
    push_ranges[0].offset = 0;
    push_ranges[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    push_ranges[1].size = sizeof(uint64_t);
    push_ranges[1].offset = sizeof(uint64_t);

    VkPipelineLayoutCreateInfo const pipeline_layout_info{
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr,           0, 0, nullptr,
        static_cast<uint32_t>(push_ranges.size()),     push_ranges.data()};

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.pipeline_layout_ci_ = pipeline_layout_info;
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, PhysicalStorageBufferStructRecursion) {
    TEST_DESCRIPTION("Make sure shader can have a buffer_reference that contains itself.");

    SetTargetApiVersion(VK_API_VERSION_1_2);
    AddRequiredExtensions(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (DeviceValidationVersion() < VK_API_VERSION_1_2) {
        GTEST_SKIP() << "At least Vulkan version 1.2 is required";
    }

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto features12 = LvlInitStruct<VkPhysicalDeviceVulkan12Features>();
    auto features2 = GetPhysicalDeviceFeatures2(features12);
    if (VK_TRUE != features12.bufferDeviceAddress) {
        GTEST_SKIP() << "VkPhysicalDeviceVulkan12Features::bufferDeviceAddress not supported and is required";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *cs_src = R"glsl(
#version 450 core
#extension GL_EXT_buffer_reference : enable

layout(buffer_reference) buffer T1;

layout(set = 0, binding = 0, std140) uniform T2 {
   layout(offset = 0) T1 a[2];
};

// This struct calls itself which needs to be properly handled in the shader validation or it will infinite loop
layout(buffer_reference, std140) buffer T1 {
   layout(offset = 0) T1 b[2];
};

void main() {}
        )glsl";

    const auto set_info = [&](CreateComputePipelineHelper &helper) {
        helper.cs_.reset(new VkShaderObj(this, cs_src, VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_2));
    };
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit | kWarningBit);
}

TEST_F(VkPositiveLayerTest, OpCopyObjectSampler) {
    TEST_DESCRIPTION("Reproduces a use case involving GL_EXT_nonuniform_qualifier and image samplers found in Doom Eternal trace");

    // https://github.com/KhronosGroup/glslang/pull/1762 appears to be the change that introduces the OpCopyObject in this context.

    SetTargetApiVersion(VK_API_VERSION_1_2);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (DeviceValidationVersion() < VK_API_VERSION_1_2) {
        GTEST_SKIP() << "At least Vulkan version 1.2 is required";
    }

    auto features12 = LvlInitStruct<VkPhysicalDeviceVulkan12Features>();
    auto features2 = GetPhysicalDeviceFeatures2(features12);
    if (VK_TRUE != features12.shaderStorageTexelBufferArrayNonUniformIndexing) {
        GTEST_SKIP()
            << "VkPhysicalDeviceVulkan12Features::shaderStorageTexelBufferArrayNonUniformIndexing not supported and is required";
    }
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *vertex_source = R"glsl(
#version 450

layout(location=0) out int idx;

void main() {
    idx = 0;
    gl_Position = vec4(0.0);
}
        )glsl";
    const VkShaderObj vs(this, vertex_source, VK_SHADER_STAGE_VERTEX_BIT);

    const char *fragment_source = R"glsl(
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(set=0, binding=0) uniform sampler s;
layout(set=0, binding=1) uniform texture2D t[1];
layout(location=0) in flat int idx;

layout(location=0) out vec4 frag_color;

void main() {
    // Using nonuniformEXT on the index into the image array creates the OpCopyObject instead of an OpLoad, which
    // was causing problems with how constants are identified.
	frag_color = texture(sampler2D(t[nonuniformEXT(idx)], s), vec2(0.0));
}

    )glsl";
    const VkShaderObj fs(this, fragment_source, VK_SHADER_STAGE_FRAGMENT_BIT, SPV_ENV_VULKAN_1_2);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.dsl_bindings_ = {
        {0, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
    };
    pipe.InitState();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, InitSwapchain) {
    TEST_DESCRIPTION("Make sure InitSwapchain is not producing anying invalid usage");

    AddSurfaceExtension();

    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());

    if (InitSwapchain()) {
        DestroySwapchain();
    }
}

TEST_F(VkPositiveLayerTest, DestroySwapchainWithBoundImages) {
    TEST_DESCRIPTION("Try destroying a swapchain which has multiple images");

    AddSurfaceExtension();
    AddRequiredExtensions(VK_KHR_BIND_MEMORY_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());
    if (!InitSwapchain()) {
        GTEST_SKIP() << "Cannot create surface or swapchain";
    }

    auto vkBindImageMemory2KHR =
        reinterpret_cast<PFN_vkBindImageMemory2KHR>(vk::GetDeviceProcAddr(m_device->device(), "vkBindImageMemory2KHR"));

    auto image_create_info = LvlInitStruct<VkImageCreateInfo>();
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = m_surface_formats[0].format;
    image_create_info.extent.width = m_surface_capabilities.minImageExtent.width;
    image_create_info.extent.height = m_surface_capabilities.minImageExtent.height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    auto image_swapchain_create_info = LvlInitStruct<VkImageSwapchainCreateInfoKHR>();
    image_swapchain_create_info.swapchain = m_swapchain;

    image_create_info.pNext = &image_swapchain_create_info;
    std::vector<vk_testing::Image> images(m_surface_capabilities.minImageCount);

    int i = 0;
    for (auto &image : images) {
        image.init_no_mem(*m_device, image_create_info);
        auto bind_swapchain_info = LvlInitStruct<VkBindImageMemorySwapchainInfoKHR>();
        bind_swapchain_info.swapchain = m_swapchain;
        bind_swapchain_info.imageIndex = i++;

        auto bind_info = LvlInitStruct<VkBindImageMemoryInfo>(&bind_swapchain_info);
        bind_info.image = image.handle();
        bind_info.memory = VK_NULL_HANDLE;
        bind_info.memoryOffset = 0;

        vkBindImageMemory2KHR(m_device->device(), 1, &bind_info);
    }
}

TEST_F(VkPositiveLayerTest, ProtectedSwapchainImageColorAttachment) {
    TEST_DESCRIPTION(
        "Make sure images from protected swapchain are considered protected image when writing to it as a color attachment");

#if !defined(ANDROID)
    // Protected swapchains are guaranteed in Android Loader
    // VK_KHR_surface_protected_capabilities is needed for other platforms
    // Without device to test with, blocking this test from non-Android platforms for now
    GTEST_SKIP() << "VK_KHR_surface_protected_capabilities test logic not implemented, skipping test for non-Android";
#endif

    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddSurfaceExtension();
    AddRequiredExtensions(VK_KHR_SURFACE_PROTECTED_CAPABILITIES_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    auto protected_memory_features = LvlInitStruct<VkPhysicalDeviceProtectedMemoryFeatures>();
    GetPhysicalDeviceFeatures2(protected_memory_features);

    if (protected_memory_features.protectedMemory == VK_FALSE) {
        GTEST_SKIP() << "protectedMemory feature not supported, skipped.";
    };

    // Turns m_commandBuffer into a unprotected command buffer
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &protected_memory_features));

    if (!InitSurface()) {
        GTEST_SKIP() << "Cannot create surface, skipping test";
    }
    InitSwapchainInfo();

    // Create protected swapchain
    VkBool32 supported;
    vk::GetPhysicalDeviceSurfaceSupportKHR(gpu(), m_device->graphics_queue_node_index_, m_surface, &supported);
    if (!supported) {
        GTEST_SKIP() << "Graphics queue does not support present, skipping test";
    }

    auto surface = m_surface;
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkSurfaceTransformFlagBitsKHR preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;

    VkSwapchainCreateInfoKHR swapchain_create_info = LvlInitStruct<VkSwapchainCreateInfoKHR>();
    swapchain_create_info.flags = VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = m_surface_capabilities.minImageCount;
    swapchain_create_info.imageFormat = m_surface_formats[0].format;
    swapchain_create_info.imageColorSpace = m_surface_formats[0].colorSpace;
    swapchain_create_info.imageExtent = {m_surface_capabilities.minImageExtent.width, m_surface_capabilities.minImageExtent.height};
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = imageUsage;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = preTransform;
    swapchain_create_info.compositeAlpha = m_surface_composite_alpha;
    swapchain_create_info.presentMode = m_surface_non_shared_present_mode;
    swapchain_create_info.clipped = VK_FALSE;
    swapchain_create_info.oldSwapchain = 0;
    swapchain_create_info.queueFamilyIndexCount = 4094967295;  // This SHOULD get ignored
    uint32_t bogus_int = 99;
    swapchain_create_info.pQueueFamilyIndices = &bogus_int;
    ASSERT_VK_SUCCESS(vk::CreateSwapchainKHR(device(), &swapchain_create_info, nullptr, &m_swapchain));

    // Get VkImage from swapchain which should be protected
    PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHR =
        (PFN_vkGetSwapchainImagesKHR)vk::GetDeviceProcAddr(m_device->handle(), "vkGetSwapchainImagesKHR");
    ASSERT_TRUE(vkGetSwapchainImagesKHR != nullptr);
    uint32_t image_count;
    std::vector<VkImage> swapchain_images;
    vkGetSwapchainImagesKHR(device(), m_swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count, VK_NULL_HANDLE);
    vkGetSwapchainImagesKHR(device(), m_swapchain, &image_count, swapchain_images.data());
    VkImage protected_image = swapchain_images.at(0);  // only need 1 image to test

    // Create a protected image view
    VkImageViewCreateInfo image_view_create_info = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        nullptr,
        0,
        protected_image,
        VK_IMAGE_VIEW_TYPE_2D,
        swapchain_create_info.imageFormat,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
         VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
    };
    vk_testing::ImageView image_view(*m_device, image_view_create_info);

    // A renderpass and framebuffer that contains a protected color image view
    VkAttachmentDescription attachments[1] = {{0, swapchain_create_info.imageFormat, VK_SAMPLE_COUNT_1_BIT,
                                               VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                               VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkAttachmentReference references[1] = {{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}};
    VkSubpassDescription subpass = {0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, 1, references, nullptr, nullptr, 0, nullptr};
    VkSubpassDependency dependency = {0,
                                      0,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_ACCESS_SHADER_WRITE_BIT,
                                      VK_DEPENDENCY_BY_REGION_BIT};
    // Use framework render pass and framebuffer so pipeline helper uses it
    m_renderPass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, nullptr, 0, 1, attachments, 1, &subpass, 1, &dependency};
    ASSERT_VK_SUCCESS(vk::CreateRenderPass(device(), &m_renderPass_info, nullptr, &m_renderPass));
    m_framebuffer_info = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                          nullptr,
                          0,
                          m_renderPass,
                          1,
                          &image_view.handle(),
                          swapchain_create_info.imageExtent.width,
                          swapchain_create_info.imageExtent.height,
                          1};
    ASSERT_VK_SUCCESS(vk::CreateFramebuffer(device(), &m_framebuffer_info, nullptr, &m_framebuffer));

    // basic pipeline to allow for a valid vkCmdDraw()
    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);
    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();

    // Create a protected command buffer/pool to use
    VkCommandPoolObj protectedCommandPool(m_device, m_device->graphics_queue_node_index_, VK_COMMAND_POOL_CREATE_PROTECTED_BIT);
    VkCommandBufferObj protectedCommandBuffer(m_device, &protectedCommandPool);

    protectedCommandBuffer.begin();
    VkRect2D render_area = {{0, 0}, swapchain_create_info.imageExtent};
    VkRenderPassBeginInfo render_pass_begin =
        LvlInitStruct<VkRenderPassBeginInfo>(nullptr, m_renderPass, m_framebuffer, render_area, 0u, nullptr);
    vk::CmdBeginRenderPass(protectedCommandBuffer.handle(), &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(protectedCommandBuffer.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    // This should be valid since the framebuffer color attachment is a protected swapchain image
    vk::CmdDraw(protectedCommandBuffer.handle(), 3, 1, 0, 0);
    vk::CmdEndRenderPass(protectedCommandBuffer.handle());
    protectedCommandBuffer.end();
}

TEST_F(VkPositiveLayerTest, ImageDrmFormatModifier) {
    // See https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/2610
    TEST_DESCRIPTION("Create image and imageView using VK_EXT_image_drm_format_modifier");

    SetTargetApiVersion(VK_API_VERSION_1_1);  // for extension dependencies
    AddRequiredExtensions(VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported.";
    }

    if (IsPlatform(kMockICD)) {
        GTEST_SKIP() << "Test not supported by MockICD, skipping tests";
    }

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());

    // we just hope that one of these formats supports modifiers
    // for more detailed checking, we could also check multi-planar formats.
    auto format_list = {
        VK_FORMAT_B8G8R8A8_UNORM,
        VK_FORMAT_B8G8R8A8_SRGB,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_R8G8B8A8_SRGB,
    };

    for (auto format : format_list) {
        std::vector<uint64_t> mods;

        // get general features and modifiers
        VkDrmFormatModifierPropertiesListEXT modp = LvlInitStruct<VkDrmFormatModifierPropertiesListEXT>();
        auto fmtp = LvlInitStruct<VkFormatProperties2>(&modp);

        vk::GetPhysicalDeviceFormatProperties2(gpu(), format, &fmtp);

        if (modp.drmFormatModifierCount > 0) {
            // the first call to vkGetPhysicalDeviceFormatProperties2 did only
            // retrieve the number of modifiers, we now have to retrieve
            // the modifiers
            std::vector<VkDrmFormatModifierPropertiesEXT> mod_props(modp.drmFormatModifierCount);
            modp.pDrmFormatModifierProperties = mod_props.data();

            vk::GetPhysicalDeviceFormatProperties2(gpu(), format, &fmtp);

            for (auto i = 0u; i < modp.drmFormatModifierCount; ++i) {
                auto &mod = modp.pDrmFormatModifierProperties[i];
                auto features = VK_FORMAT_FEATURE_TRANSFER_DST_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

                if ((mod.drmFormatModifierTilingFeatures & features) != features) {
                    continue;
                }

                mods.push_back(mod.drmFormatModifier);
            }
        }

        if (mods.empty()) {
            continue;
        }

        // create image
        auto ci = LvlInitStruct<VkImageCreateInfo>();
        ci.flags = 0;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = format;
        ci.extent = {128, 128, 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
        ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageDrmFormatModifierListCreateInfoEXT mod_list = LvlInitStruct<VkImageDrmFormatModifierListCreateInfoEXT>();
        mod_list.pDrmFormatModifiers = mods.data();
        mod_list.drmFormatModifierCount = mods.size();
        ci.pNext = &mod_list;

        VkImage image;
        VkResult err = vk::CreateImage(device(), &ci, nullptr, &image);
        ASSERT_VK_SUCCESS(err);

        // bind memory
        VkPhysicalDeviceMemoryProperties phys_mem_props;
        vk::GetPhysicalDeviceMemoryProperties(gpu(), &phys_mem_props);
        VkMemoryRequirements mem_reqs;
        vk::GetImageMemoryRequirements(device(), image, &mem_reqs);
        VkDeviceMemory mem_obj = VK_NULL_HANDLE;
        VkMemoryPropertyFlagBits mem_props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        for (uint32_t type = 0; type < phys_mem_props.memoryTypeCount; type++) {
            if ((mem_reqs.memoryTypeBits & (1 << type)) &&
                ((phys_mem_props.memoryTypes[type].propertyFlags & mem_props) == mem_props)) {
                VkMemoryAllocateInfo alloc_info = LvlInitStruct<VkMemoryAllocateInfo>();
                alloc_info.allocationSize = mem_reqs.size;
                alloc_info.memoryTypeIndex = type;
                ASSERT_VK_SUCCESS(vk::AllocateMemory(device(), &alloc_info, nullptr, &mem_obj));
                break;
            }
        }

        ASSERT_NE((VkDeviceMemory)VK_NULL_HANDLE, mem_obj);
        ASSERT_VK_SUCCESS(vk::BindImageMemory(device(), image, mem_obj, 0));

        // create image view
        VkImageViewCreateInfo ivci = {
            VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            nullptr,
            0,
            image,
            VK_IMAGE_VIEW_TYPE_2D,
            format,
            {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
             VK_COMPONENT_SWIZZLE_IDENTITY},
            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1},
        };

        CreateImageViewTest(*this, &ivci);

        // for more detailed checking, we could export the image to dmabuf
        // and then import it again (using VkImageDrmFormatModifierExplicitCreateInfoEXT)

        vk::FreeMemory(device(), mem_obj, nullptr);
        vk::DestroyImage(device(), image, nullptr);
    }
}

TEST_F(VkPositiveLayerTest, AllowedDuplicateStype) {
    TEST_DESCRIPTION("Pass duplicate structs to whose vk.xml definition contains allowduplicate=true");

    VkInstance instance;

    VkInstanceCreateInfo ici = LvlInitStruct<VkInstanceCreateInfo>();
    ici.enabledLayerCount = instance_layers_.size();
    ici.ppEnabledLayerNames = instance_layers_.data();

    auto dbgUtils0 = LvlInitStruct<VkDebugUtilsMessengerCreateInfoEXT>();
    auto dbgUtils1 = LvlInitStruct<VkDebugUtilsMessengerCreateInfoEXT>(&dbgUtils0);
    ici.pNext = &dbgUtils1;

    ASSERT_VK_SUCCESS(vk::CreateInstance(&ici, nullptr, &instance));

    ASSERT_NO_FATAL_FAILURE(vk::DestroyInstance(instance, nullptr));
}

TEST_F(VkPositiveLayerTest, MeshShaderOnly) {
    TEST_DESCRIPTION("Test using a mesh shader without a vertex shader.");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_NV_MESH_SHADER_EXTENSION_NAME);
    AddRequiredExtensions(VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    // Create a device that enables mesh_shader
    auto mesh_shader_features = LvlInitStruct<VkPhysicalDeviceMeshShaderFeaturesNV>();
    auto features2 = GetPhysicalDeviceFeatures2(mesh_shader_features);
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    if (mesh_shader_features.meshShader != VK_TRUE) {
        GTEST_SKIP() << "Mesh shader feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    static const char meshShaderText[] = R"glsl(
        #version 450
        #extension GL_NV_mesh_shader : require
        layout(local_size_x = 1) in;
        layout(max_vertices = 3) out;
        layout(max_primitives = 1) out;
        layout(triangles) out;
        void main() {
              gl_MeshVerticesNV[0].gl_Position = vec4(-1.0, -1.0, 0, 1);
              gl_MeshVerticesNV[1].gl_Position = vec4( 1.0, -1.0, 0, 1);
              gl_MeshVerticesNV[2].gl_Position = vec4( 0.0,  1.0, 0, 1);
              gl_PrimitiveIndicesNV[0] = 0;
              gl_PrimitiveIndicesNV[1] = 1;
              gl_PrimitiveIndicesNV[2] = 2;
              gl_PrimitiveCountNV = 1;
        }
    )glsl";

    VkShaderObj ms(this, meshShaderText, VK_SHADER_STAGE_MESH_BIT_NV);
    VkShaderObj fs(this, bindStateFragShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper helper(*this);
    helper.InitInfo();
    helper.shader_stages_ = {ms.GetStageCreateInfo(), fs.GetStageCreateInfo()};

    // Ensure pVertexInputState and pInputAssembly state are null, as these should be ignored.
    helper.gp_ci_.pVertexInputState = nullptr;
    helper.gp_ci_.pInputAssemblyState = nullptr;

    helper.InitState();

    helper.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, CopyImageSubresource) {
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
    VkImageObj image(m_device);
    auto image_ci = VkImageObj::ImageCreateInfo2D(128, 128, 2, 5, format, usage, VK_IMAGE_TILING_OPTIMAL);
    image.InitNoLayout(image_ci);
    ASSERT_TRUE(image.initialized());

    VkImageSubresourceLayers src_layer{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    VkImageSubresourceLayers dst_layer{VK_IMAGE_ASPECT_COLOR_BIT, 1, 3, 1};
    VkOffset3D zero_offset{0, 0, 0};
    VkExtent3D full_extent{128 / 2, 128 / 2, 1};  // <-- image type is 2D
    VkImageCopy region = {src_layer, zero_offset, dst_layer, zero_offset, full_extent};
    auto init_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto src_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    auto dst_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    auto final_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    m_commandBuffer->begin();

    auto cb = m_commandBuffer->handle();

    VkImageSubresourceRange src_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageMemoryBarrier image_barriers[2];

    image_barriers[0] = LvlInitStruct<VkImageMemoryBarrier>();
    image_barriers[0].srcAccessMask = 0;
    image_barriers[0].dstAccessMask = 0;
    image_barriers[0].image = image.handle();
    image_barriers[0].subresourceRange = src_range;
    image_barriers[0].oldLayout = init_layout;
    image_barriers[0].newLayout = dst_layout;

    vk::CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1,
                           image_barriers);
    VkClearColorValue clear_color{};
    vk::CmdClearColorImage(cb, image.handle(), dst_layout, &clear_color, 1, &src_range);
    m_commandBuffer->end();

    auto submit_info = LvlInitStruct<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_commandBuffer->handle();

    vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vk::QueueWaitIdle(m_device->m_queue);

    m_commandBuffer->begin();

    image_barriers[0].oldLayout = dst_layout;
    image_barriers[0].newLayout = src_layout;

    VkImageSubresourceRange dst_range{VK_IMAGE_ASPECT_COLOR_BIT, 1, 1, 3, 1};
    image_barriers[1] = LvlInitStruct<VkImageMemoryBarrier>();
    image_barriers[1].srcAccessMask = 0;
    image_barriers[1].dstAccessMask = 0;
    image_barriers[1].image = image.handle();
    image_barriers[1].subresourceRange = dst_range;
    image_barriers[1].oldLayout = init_layout;
    image_barriers[1].newLayout = dst_layout;

    vk::CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 2,
                           image_barriers);

    vk::CmdCopyImage(cb, image.handle(), src_layout, image.handle(), dst_layout, 1, &region);

    image_barriers[0].oldLayout = src_layout;
    image_barriers[0].newLayout = final_layout;
    image_barriers[1].oldLayout = dst_layout;
    image_barriers[1].newLayout = final_layout;
    vk::CmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 2,
                           image_barriers);
    m_commandBuffer->end();

    vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vk::QueueWaitIdle(m_device->m_queue);
}

TEST_F(VkPositiveLayerTest, ImageDescriptorSubresourceLayout) {
    AddRequiredExtensions(VK_KHR_MAINTENANCE_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));

    ASSERT_NO_FATAL_FAILURE(InitViewport());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    OneOffDescriptorSet descriptor_set(m_device,
                                       {
                                           {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr},
                                       });
    VkDescriptorSet descriptorSet = descriptor_set.set_;

    const VkPipelineLayoutObj pipeline_layout(m_device, {&descriptor_set.layout_});

    // Create image, view, and sampler
    const VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkImageObj image(m_device);
    auto usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    auto image_ci = VkImageObj::ImageCreateInfo2D(128, 128, 1, 5, format, usage, VK_IMAGE_TILING_OPTIMAL);
    image.Init(image_ci);
    ASSERT_TRUE(image.initialized());

    VkImageSubresourceRange view_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 3, 1};
    VkImageSubresourceRange first_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkImageSubresourceRange full_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 5};
    vk_testing::ImageView view;
    auto image_view_create_info = LvlInitStruct<VkImageViewCreateInfo>();
    image_view_create_info.image = image.handle();
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange = view_range;

    view.init(*m_device, image_view_create_info);
    ASSERT_TRUE(view.initialized());

    // Create Sampler
    vk_testing::Sampler sampler;
    VkSamplerCreateInfo sampler_ci = SafeSaneSamplerCreateInfo();
    sampler.init(*m_device, sampler_ci);
    ASSERT_TRUE(sampler.initialized());

    // Setup structure for descriptor update with sampler, for update in do_test below
    VkDescriptorImageInfo img_info = {};
    img_info.sampler = sampler.handle();

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = descriptorSet;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &img_info;

    // Create PSO to be used for draw-time errors below
    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragSamplerShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineObj pipe(m_device);
    pipe.AddShader(&vs);
    pipe.AddShader(&fs);
    pipe.AddDefaultColorAttachment();
    pipe.CreateVKPipeline(pipeline_layout.handle(), renderPass());

    VkViewport viewport = {0, 0, 16, 16, 0, 1};
    VkRect2D scissor = {{0, 0}, {16, 16}};

    VkCommandBufferObj cmd_buf(m_device, m_commandPool);

    VkSubmitInfo submit_info = LvlInitStruct<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf.handle();

    enum TestType {
        kInternal,  // Image layout mismatch is *within* a given command buffer
        kExternal   // Image layout mismatch is with the current state of the image, found at QueueSubmit
    };
    std::array<TestType, 2> test_list = {{kInternal, kExternal}};

    auto do_test = [&](VkImageObj *image, vk_testing::ImageView *view, VkImageAspectFlags aspect_mask,
                       VkImageLayout descriptor_layout) {
        // Set up the descriptor
        img_info.imageView = view->handle();
        img_info.imageLayout = descriptor_layout;
        vk::UpdateDescriptorSets(m_device->device(), 1, &descriptor_write, 0, NULL);

        for (TestType test_type : test_list) {
            auto init_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            auto image_barrier = LvlInitStruct<VkImageMemoryBarrier>();

            cmd_buf.begin();
            image_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            image_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            image_barrier.image = image->handle();
            image_barrier.subresourceRange = full_range;
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_barrier.newLayout = init_layout;

            cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &image_barrier);

            image_barrier.subresourceRange = first_range;
            image_barrier.oldLayout = init_layout;
            image_barrier.newLayout = descriptor_layout;
            cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &image_barrier);

            image_barrier.subresourceRange = view_range;
            image_barrier.oldLayout = init_layout;
            image_barrier.newLayout = descriptor_layout;
            cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &image_barrier);

            if (test_type == kExternal) {
                // The image layout is external to the command buffer we are recording to test.  Submit to push to instance scope.
                cmd_buf.end();
                vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
                vk::QueueWaitIdle(m_device->m_queue);
                cmd_buf.begin();
            }

            cmd_buf.BeginRenderPass(m_renderPassBeginInfo);
            vk::CmdBindPipeline(cmd_buf.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.handle());
            vk::CmdBindDescriptorSets(cmd_buf.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout.handle(), 0, 1,
                                      &descriptorSet, 0, NULL);
            vk::CmdSetViewport(cmd_buf.handle(), 0, 1, &viewport);
            vk::CmdSetScissor(cmd_buf.handle(), 0, 1, &scissor);

            cmd_buf.Draw(1, 0, 0, 0);

            cmd_buf.EndRenderPass();
            cmd_buf.end();

            // Submit cmd buffer
            vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
            vk::QueueWaitIdle(m_device->m_queue);
        }
    };
    do_test(&image, &view, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VkPositiveLayerTest, ExtensionsInCreateInstance) {
    TEST_DESCRIPTION("Test to see if instance extensions are called during CreateInstance.");

    // See https://github.com/KhronosGroup/Vulkan-Loader/issues/537 for more details.
    // This is specifically meant to ensure a crash encountered in profiles does not occur, but also to
    // attempt to ensure that no extension calls have been added to CreateInstance hooks.
    // NOTE: it is certainly possible that a layer will call an extension during the Createinstance hook
    //       and the loader will _not_ crash (e.g., nvidia, android seem to not crash in this case, but AMD does).
    //       So, this test will only catch an erroneous extension _if_ run on HW/a driver that crashes in this use
    //       case.

    for (const auto &ext : InstanceExtensions::get_info_map()) {
        // Add all "real" instance extensions
        if (InstanceExtensionSupported(ext.first.c_str())) {
            bool version_required = false;
            for (const auto &req : ext.second.requirements) {
                std::string name(req.name);
                if (name.find("VK_VERSION") != std::string::npos) {
                    version_required = true;
                    break;
                }
            }
            if (!version_required) {
                m_instance_extension_names.emplace_back(ext.first.c_str());
            }
        }
    }

    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
}

TEST_F(VkPositiveLayerTest, ImageDescriptor3D2DSubresourceLayout) {
    TEST_DESCRIPTION("Verify renderpass layout transitions for a 2d ImageView created from a 3d Image.");
    SetTargetApiVersion(VK_API_VERSION_1_1);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));

    ASSERT_NO_FATAL_FAILURE(InitViewport());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    OneOffDescriptorSet descriptor_set(m_device,
                                       {
                                           {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_ALL, nullptr},
                                       });
    VkDescriptorSet descriptorSet = descriptor_set.set_;

    const VkPipelineLayoutObj pipeline_layout(m_device, {&descriptor_set.layout_});

    // Create image, view, and sampler
    const VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    VkImageObj image_3d(m_device);
    VkImageObj other_image(m_device);
    auto usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    static const uint32_t kWidth = 128;
    static const uint32_t kHeight = 128;

    auto image_ci_3d = LvlInitStruct<VkImageCreateInfo>();
    image_ci_3d.flags = VK_IMAGE_CREATE_2D_ARRAY_COMPATIBLE_BIT;
    image_ci_3d.imageType = VK_IMAGE_TYPE_3D;
    image_ci_3d.format = format;
    image_ci_3d.extent.width = kWidth;
    image_ci_3d.extent.height = kHeight;
    image_ci_3d.extent.depth = 8;
    image_ci_3d.mipLevels = 1;
    image_ci_3d.arrayLayers = 1;
    image_ci_3d.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci_3d.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci_3d.usage = usage;
    image_3d.Init(image_ci_3d);
    ASSERT_TRUE(image_3d.initialized());

    other_image.Init(kWidth, kHeight, 1, format, usage, VK_IMAGE_TILING_OPTIMAL, 0);
    ASSERT_TRUE(other_image.initialized());

    // The image view is a 2D slice of the 3D image at depth = 4, which we request by
    // asking for arrayLayer = 4
    VkImageSubresourceRange view_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 4, 1};
    // But, the spec says:
    //    Automatic layout transitions apply to the entire image subresource attached
    //    to the framebuffer. If the attachment view is a 2D or 2D array view of a
    //    3D image, even if the attachment view only refers to a subset of the slices
    //    of the selected mip level of the 3D image, automatic layout transitions apply
    //    to the entire subresource referenced which is the entire mip level in this case.
    VkImageSubresourceRange full_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vk_testing::ImageView view_2d, other_view;
    auto image_view_create_info = LvlInitStruct<VkImageViewCreateInfo>();
    image_view_create_info.image = image_3d.handle();
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange = view_range;

    view_2d.init(*m_device, image_view_create_info);
    ASSERT_TRUE(view_2d.initialized());

    image_view_create_info.image = other_image.handle();
    image_view_create_info.subresourceRange = full_range;
    other_view.init(*m_device, image_view_create_info);
    ASSERT_TRUE(other_view.initialized());

    std::vector<VkAttachmentDescription> attachments = {
        {0, format, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };

    std::vector<VkAttachmentReference> color = {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };

    VkSubpassDescription subpass = {
        0, VK_PIPELINE_BIND_POINT_GRAPHICS, 0, nullptr, (uint32_t)color.size(), color.data(), nullptr, nullptr, 0, nullptr};

    std::vector<VkSubpassDependency> deps = {
        {VK_SUBPASS_EXTERNAL, 0,
         (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
          VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT),
         (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
          VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT),
         (VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_TRANSFER_WRITE_BIT),
         (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_MEMORY_WRITE_BIT), 0},
        {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         (VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT), VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT), 0},
    };

    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                   nullptr,
                                   0,
                                   (uint32_t)attachments.size(),
                                   attachments.data(),
                                   1,
                                   &subpass,
                                   (uint32_t)deps.size(),
                                   deps.data()};
    // Create Sampler
    vk_testing::Sampler sampler;
    VkSamplerCreateInfo sampler_ci = SafeSaneSamplerCreateInfo();
    sampler.init(*m_device, sampler_ci);
    ASSERT_TRUE(sampler.initialized());

    // Setup structure for descriptor update with sampler, for update in do_test below
    VkDescriptorImageInfo img_info = {};
    img_info.sampler = sampler.handle();

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = descriptorSet;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptor_write.pImageInfo = &img_info;

    vk_testing::RenderPass rp(*m_device, rpci);

    // Create PSO to be used for draw-time errors below
    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, bindStateFragSamplerShaderText, VK_SHADER_STAGE_FRAGMENT_BIT);
    VkPipelineObj pipe(m_device);
    pipe.AddShader(&vs);
    pipe.AddShader(&fs);
    pipe.AddDefaultColorAttachment();
    pipe.CreateVKPipeline(pipeline_layout.handle(), rp.handle());

    VkViewport viewport = {0, 0, kWidth, kHeight, 0, 1};
    VkRect2D scissor = {{0, 0}, {kWidth, kHeight}};

    VkCommandBufferObj cmd_buf(m_device, m_commandPool);

    VkSubmitInfo submit_info = LvlInitStruct<VkSubmitInfo>();
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &cmd_buf.handle();

    enum TestType {
        kInternal,  // Image layout mismatch is *within* a given command buffer
        kExternal   // Image layout mismatch is with the current state of the image, found at QueueSubmit
    };
    std::array<TestType, 2> test_list = {{kInternal, kExternal}};

    auto do_test = [&](VkImageObj *image, vk_testing::ImageView *view, VkImageObj *o_image, vk_testing::ImageView *o_view,
                       VkImageAspectFlags aspect_mask, VkImageLayout descriptor_layout) {
        // Set up the descriptor
        img_info.imageView = o_view->handle();
        img_info.imageLayout = descriptor_layout;
        vk::UpdateDescriptorSets(m_device->device(), 1, &descriptor_write, 0, NULL);

        for (TestType test_type : test_list) {
            auto image_barrier = LvlInitStruct<VkImageMemoryBarrier>();

            VkFramebufferCreateInfo fbci = {
                VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, nullptr, 0, rp.handle(), 1, &view->handle(), kWidth, kHeight, 1};
            vk_testing::Framebuffer fb(*m_device, fbci);

            cmd_buf.begin();
            image_barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            image_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
            image_barrier.image = image->handle();
            image_barrier.subresourceRange = full_range;
            image_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            image_barrier.newLayout = descriptor_layout;

            cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &image_barrier);
            image_barrier.image = o_image->handle();
            cmd_buf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                                    nullptr, 1, &image_barrier);

            if (test_type == kExternal) {
                // The image layout is external to the command buffer we are recording to test.  Submit to push to instance scope.
                cmd_buf.end();
                vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
                vk::QueueWaitIdle(m_device->m_queue);
                cmd_buf.begin();
            }

            m_renderPassBeginInfo.renderPass = rp.handle();
            m_renderPassBeginInfo.framebuffer = fb.handle();
            m_renderPassBeginInfo.renderArea = {{0, 0}, {kWidth, kHeight}};

            cmd_buf.BeginRenderPass(m_renderPassBeginInfo);
            vk::CmdBindPipeline(cmd_buf.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.handle());
            vk::CmdBindDescriptorSets(cmd_buf.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout.handle(), 0, 1,
                                      &descriptorSet, 0, NULL);
            vk::CmdSetViewport(cmd_buf.handle(), 0, 1, &viewport);
            vk::CmdSetScissor(cmd_buf.handle(), 0, 1, &scissor);

            cmd_buf.Draw(1, 0, 0, 0);

            cmd_buf.EndRenderPass();
            cmd_buf.end();

            // Submit cmd buffer
            vk::QueueSubmit(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
            vk::QueueWaitIdle(m_device->m_queue);
        }
    };
    do_test(&image_3d, &view_2d, &other_image, &other_view, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

TEST_F(VkPositiveLayerTest, RenderPassInputResolve) {
    TEST_DESCRIPTION("Create render pass where input attachment == resolve attachment");

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddOptionalExtensions(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    const bool rp2Supported = IsExtensionsEnabled(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());

    std::vector<VkAttachmentDescription> attachments = {
        // input attachments
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL},
        // color attachments
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_4_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        // resolve attachment
        {0, VK_FORMAT_R8G8B8A8_UNORM, VK_SAMPLE_COUNT_1_BIT, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE,
         VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };

    std::vector<VkAttachmentReference> input = {
        {0, VK_IMAGE_LAYOUT_GENERAL},
    };
    std::vector<VkAttachmentReference> color = {
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };
    std::vector<VkAttachmentReference> resolve = {
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {VK_ATTACHMENT_UNUSED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
    };

    VkSubpassDescription subpass = {0,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    (uint32_t)input.size(),
                                    input.data(),
                                    (uint32_t)color.size(),
                                    color.data(),
                                    resolve.data(),
                                    nullptr,
                                    0,
                                    nullptr};

    VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                   nullptr,
                                   0,
                                   (uint32_t)attachments.size(),
                                   attachments.data(),
                                   1,
                                   &subpass,
                                   0,
                                   nullptr};

    PositiveTestRenderPassCreate(m_errorMonitor, m_device->device(), &rpci, rp2Supported);
}

TEST_F(VkPositiveLayerTest, SpecializationUnused) {
    TEST_DESCRIPTION("Make sure an unused spec constant is valid to us");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // layout (constant_id = 2) const int a = 3;
    const char *cs_src = R"(
               OpCapability Shader
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 450
               OpDecorate %a SpecId 2
       %void = OpTypeVoid
       %func = OpTypeFunction %void
        %int = OpTypeInt 32 1
          %a = OpSpecConstant %int 3
       %main = OpFunction %void None %func
      %label = OpLabel
               OpReturn
               OpFunctionEnd
        )";

    VkSpecializationMapEntry entries[4] = {
        {0, 0, 1},  // unused
        {1, 0, 1},  // usued
        {2, 0, 4},  // OpTypeInt 32
        {3, 0, 4},  // usued
    };

    int32_t data = 0;
    VkSpecializationInfo specialization_info = {
        4,
        entries,
        1 * sizeof(decltype(data)),
        &data,
    };

    const auto set_info = [&](CreateComputePipelineHelper &helper) {
        helper.cs_.reset(
            new VkShaderObj(this, cs_src, VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM, &specialization_info));
    };
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit | kWarningBit);

    // Even if the ID is never seen in VkSpecializationMapEntry the OpSpecConstant will use the default and still is valid
    specialization_info.mapEntryCount = 1;
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit | kWarningBit);

    // try another random unused value other than zero
    entries[0].constantID = 100;
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit | kWarningBit);
}

TEST_F(VkPositiveLayerTest, FillBufferCmdPoolTransferQueue) {
    TEST_DESCRIPTION(
        "Use a command buffer with vkCmdFillBuffer that was allocated from a command pool that does not support graphics or "
        "compute opeartions");

    SetTargetApiVersion(VK_API_VERSION_1_1);
    ASSERT_NO_FATAL_FAILURE(Init());
    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    const std::optional<uint32_t> transfer = m_device->QueueFamilyWithoutCapabilities(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);
    if (!transfer) {
        GTEST_SKIP() << "Required queue families not present (non-graphics non-compute capable required)";
    }
    VkQueueObj *queue = m_device->queue_family_queues(transfer.value())[0].get();

    VkCommandPoolObj pool(m_device, transfer.value(), VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    VkCommandBufferObj cb(m_device, &pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, queue);

    VkMemoryPropertyFlags reqs = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    VkBufferObj buffer;
    buffer.init_as_dst(*m_device, (VkDeviceSize)20, reqs);

    cb.begin();
    cb.FillBuffer(buffer.handle(), 0, 12, 0x11111111);
    cb.end();
}

TEST_F(VkPositiveLayerTest, ShaderAtomicInt64) {
    TEST_DESCRIPTION("Test VK_KHR_shader_atomic_int64.");
    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto atomic_int64_features = LvlInitStruct<VkPhysicalDeviceShaderAtomicInt64Features>();
    auto features2 = GetPhysicalDeviceFeatures2(atomic_int64_features);
    if (features2.features.shaderInt64 == VK_FALSE) {
        GTEST_SKIP() << "shaderInt64 feature not supported";
    }

    // at least shaderBufferInt64Atomics is guaranteed to be supported
    if (atomic_int64_features.shaderBufferInt64Atomics == VK_FALSE) {
        GTEST_SKIP()
            << "shaderBufferInt64Atomics feature is required for VK_KHR_shader_atomic_int64 but not expose, likely driver bug";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required.";
    }

    std::string cs_base = R"glsl(
        #version 450
        #extension GL_EXT_shader_explicit_arithmetic_types_int64 : enable
        #extension GL_EXT_shader_atomic_int64 : enable
        #extension GL_KHR_memory_scope_semantics : enable
        shared uint64_t x;
        layout(set = 0, binding = 0) buffer ssbo { uint64_t y; };
        void main() {
    )glsl";

    // clang-format off
    // StorageBuffer storage class
    std::string cs_storage_buffer = cs_base + R"glsl(
           atomicAdd(y, 1);
        }
    )glsl";

    // StorageBuffer storage class using AtomicStore
    // atomicStore is slightly different than other atomics, so good edge case
    std::string cs_store = cs_base + R"glsl(
           atomicStore(y, 1ul, gl_ScopeDevice, gl_StorageSemanticsBuffer, gl_SemanticsRelaxed);
        }
    )glsl";

    // Workgroup storage class
    std::string cs_workgroup = cs_base + R"glsl(
           atomicAdd(x, 1);
           barrier();
           y = x + 1;
        }
    )glsl";
    // clang-format on

    const char *current_shader = nullptr;
    const auto set_info = [&](CreateComputePipelineHelper &helper) {
        // Requires SPIR-V 1.3 for SPV_KHR_storage_buffer_storage_class
        helper.cs_.reset(new VkShaderObj(this, current_shader, VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_1));
        helper.dsl_bindings_ = {{0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr}};
    };

    current_shader = cs_storage_buffer.c_str();
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);

    current_shader = cs_store.c_str();
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);

    if (atomic_int64_features.shaderSharedInt64Atomics == VK_TRUE) {
        current_shader = cs_workgroup.c_str();
        CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, TopologyAtRasterizer) {
    TEST_DESCRIPTION("Test topology set when creating a pipeline with tessellation and geometry shader.");

    ASSERT_NO_FATAL_FAILURE(Init());

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    if (!m_device->phy().features().tessellationShader) {
        GTEST_SKIP() << "Device does not support tessellation shaders";
    }

    char const *tcsSource = R"glsl(
        #version 450
        layout(vertices = 3) out;
        void main(){
           gl_TessLevelOuter[0] = gl_TessLevelOuter[1] = gl_TessLevelOuter[2] = 1;
           gl_TessLevelInner[0] = 1;
        }
    )glsl";
    char const *tesSource = R"glsl(
        #version 450
        layout(isolines, equal_spacing, cw) in;
        void main(){
           gl_Position.xyz = gl_TessCoord;
           gl_Position.w = 1.0f;
        }
    )glsl";
    static char const *gsSource = R"glsl(
        #version 450
        layout (triangles) in;
        layout (triangle_strip) out;
        layout (max_vertices = 1) out;
        void main() {
           gl_Position = vec4(1.0, 0.5, 0.5, 0.0);
           EmitVertex();
        }
    )glsl";
    VkShaderObj tcs(this, tcsSource, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT);
    VkShaderObj tes(this, tesSource, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT);
    VkShaderObj gs(this, gsSource, VK_SHADER_STAGE_GEOMETRY_BIT);

    VkPipelineInputAssemblyStateCreateInfo iasci{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                 VK_PRIMITIVE_TOPOLOGY_PATCH_LIST, VK_FALSE};

    VkPipelineTessellationStateCreateInfo tsci{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, nullptr, 0, 3};

    VkDynamicState dyn_state = VK_DYNAMIC_STATE_LINE_WIDTH;
    VkPipelineDynamicStateCreateInfo dyn_state_ci = LvlInitStruct<VkPipelineDynamicStateCreateInfo>();
    dyn_state_ci.dynamicStateCount = 1;
    dyn_state_ci.pDynamicStates = &dyn_state;

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.gp_ci_.pTessellationState = &tsci;
    pipe.gp_ci_.pInputAssemblyState = &iasci;
    pipe.shader_stages_.emplace_back(gs.GetStageCreateInfo());
    pipe.shader_stages_.emplace_back(tcs.GetStageCreateInfo());
    pipe.shader_stages_.emplace_back(tes.GetStageCreateInfo());
    pipe.InitState();
    pipe.dyn_state_ci_ = dyn_state_ci;
    pipe.CreateGraphicsPipeline();

    VkRenderPassBeginInfo rpbi = LvlInitStruct<VkRenderPassBeginInfo>();
    rpbi.renderPass = m_renderPass;
    rpbi.framebuffer = m_framebuffer;
    rpbi.renderArea.offset.x = 0;
    rpbi.renderArea.offset.y = 0;
    rpbi.renderArea.extent.width = 32;
    rpbi.renderArea.extent.height = 32;
    rpbi.clearValueCount = static_cast<uint32_t>(m_renderPassClearValues.size());
    rpbi.pClearValues = m_renderPassClearValues.data();

    m_commandBuffer->begin();
    vk::CmdBeginRenderPass(m_commandBuffer->handle(), &rpbi, VK_SUBPASS_CONTENTS_INLINE);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    vk::CmdDraw(m_commandBuffer->handle(), 4, 1, 0, 0);
    vk::CmdEndRenderPass(m_commandBuffer->handle());
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, TestPervertexNVShaderAttributes) {
    TEST_DESCRIPTION("Test using TestRasterizationStateStreamCreateInfoEXT with invalid rasterizationStream.");

    AddRequiredExtensions(VK_NV_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV fragment_shader_barycentric_features =
        LvlInitStruct<VkPhysicalDeviceFragmentShaderBarycentricFeaturesNV>();
    fragment_shader_barycentric_features.fragmentShaderBarycentric = VK_TRUE;
    auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2KHR>(&fragment_shader_barycentric_features);
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *vsSource = R"glsl(
                #version 450

                layout(location = 0) out PerVertex {
                    vec3 vtxPos;
                } outputs;

                vec2 triangle_positions[3] = vec2[](
                    vec2(0.5, -0.5),
                    vec2(0.5, 0.5),
                    vec2(-0.5, 0.5)
                );

                void main() {
                    gl_Position = vec4(triangle_positions[gl_VertexIndex], 0.0, 1.0);
                    outputs.vtxPos = gl_Position.xyz;
                }
            )glsl";

    char const *fsSource = R"glsl(
                #version 450

                #extension GL_NV_fragment_shader_barycentric : enable

                layout(location = 0) in pervertexNV PerVertex {
                    vec3 vtxPos;
                } inputs[3];

                layout(location = 0) out vec4 out_color;

                void main() {
                    vec3 b = gl_BaryCoordNV;
                    if (b.x > b.y && b.x > b.z) {
                        out_color = vec4(inputs[0].vtxPos, 1.0);
                    }
                    else if(b.y > b.z) {
                        out_color = vec4(inputs[1].vtxPos, 1.0);
                    }
                    else {
                        out_color = vec4(inputs[2].vtxPos, 1.0);
                    }
                }
            )glsl";

    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, LineTopologyClasses) {
    TEST_DESCRIPTION("Check different line topologies within the same topology class");

    SetTargetApiVersion(VK_API_VERSION_1_1);

    AddRequiredExtensions(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto extended_dynamic_state_features = LvlInitStruct<VkPhysicalDeviceExtendedDynamicStateFeaturesEXT>();
    GetPhysicalDeviceFeatures2(extended_dynamic_state_features);

    if (!extended_dynamic_state_features.extendedDynamicState) {
        GTEST_SKIP() << "Test requires (unsupported) extendedDynamicState";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &extended_dynamic_state_features));

    auto vkCmdSetPrimitiveTopologyEXT = reinterpret_cast<PFN_vkCmdSetPrimitiveTopologyEXT>(
        vk::GetDeviceProcAddr(m_device->device(), "vkCmdSetPrimitiveTopologyEXT"));

    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const VkDynamicState dyn_states[1] = {
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY_EXT,
    };

    // Verify each vkCmdSet command
    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    auto dyn_state_ci = LvlInitStruct<VkPipelineDynamicStateCreateInfo>();
    dyn_state_ci.dynamicStateCount = size(dyn_states);
    dyn_state_ci.pDynamicStates = dyn_states;
    pipe.dyn_state_ci_ = dyn_state_ci;
    pipe.vi_ci_.vertexBindingDescriptionCount = 1;
    VkVertexInputBindingDescription inputBinding = {0, sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX};
    pipe.vi_ci_.pVertexBindingDescriptions = &inputBinding;
    pipe.vi_ci_.vertexAttributeDescriptionCount = 1;
    VkVertexInputAttributeDescription attribute = {0, 0, VK_FORMAT_R32_SFLOAT, 0};
    pipe.vi_ci_.pVertexAttributeDescriptions = &attribute;
    pipe.ia_ci_.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
    pipe.InitState();
    pipe.CreateGraphicsPipeline();

    const float vbo_data[3] = {0};
    VkConstantBufferObj vb(m_device, sizeof(vbo_data), reinterpret_cast<const void *>(&vbo_data),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

    VkCommandBufferObj cb(m_device, m_commandPool);
    cb.begin();
    cb.BeginRenderPass(m_renderPassBeginInfo);

    vk::CmdBindPipeline(cb.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    cb.BindVertexBuffer(&vb, 0, 0);
    vkCmdSetPrimitiveTopologyEXT(cb.handle(), VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY);
    vk::CmdDraw(cb.handle(), 1, 1, 0, 0);

    cb.EndRenderPass();

    cb.end();
}

TEST_F(VkPositiveLayerTest, MutableStorageImageFormatWriteForFormat) {
    TEST_DESCRIPTION("Create a shader writing a storage image without an image format");

    // need to be compatible to use VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT
    const VkFormat image_format = VK_FORMAT_B8G8R8A8_SRGB;
    const VkFormat image_view_format = VK_FORMAT_R32_SFLOAT;

    AddRequiredExtensions(VK_KHR_FORMAT_FEATURE_FLAGS_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, nullptr, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT));

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    PFN_vkSetPhysicalDeviceFormatProperties2EXT fpvkSetPhysicalDeviceFormatProperties2EXT = nullptr;
    PFN_vkGetOriginalPhysicalDeviceFormatProperties2EXT fpvkGetOriginalPhysicalDeviceFormatProperties2EXT = nullptr;
    if (!LoadDeviceProfileLayer(fpvkSetPhysicalDeviceFormatProperties2EXT, fpvkGetOriginalPhysicalDeviceFormatProperties2EXT)) {
        GTEST_SKIP() << "Failed to load device profile layer.";
    }

    auto fmt_props_3 = LvlInitStruct<VkFormatProperties3>();
    auto fmt_props = LvlInitStruct<VkFormatProperties2>(&fmt_props_3);

    fpvkGetOriginalPhysicalDeviceFormatProperties2EXT(gpu(), image_format, &fmt_props);
    fmt_props.formatProperties.optimalTilingFeatures =
        (fmt_props.formatProperties.optimalTilingFeatures & ~VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT);
    fmt_props_3.optimalTilingFeatures = (fmt_props_3.optimalTilingFeatures & ~VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT);
    fmt_props_3.optimalTilingFeatures = (fmt_props_3.optimalTilingFeatures & ~VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT);
    fpvkSetPhysicalDeviceFormatProperties2EXT(gpu(), image_format, fmt_props);

    fpvkGetOriginalPhysicalDeviceFormatProperties2EXT(gpu(), image_view_format, &fmt_props);
    fmt_props.formatProperties.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT;
    fmt_props_3.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_IMAGE_BIT;
    fmt_props_3.optimalTilingFeatures |= VK_FORMAT_FEATURE_2_STORAGE_WRITE_WITHOUT_FORMAT_BIT;
    fpvkSetPhysicalDeviceFormatProperties2EXT(gpu(), image_view_format, fmt_props);

    // Make sure compute pipeline has a compute shader stage set
    const char *csSource = R"(
                  OpCapability Shader
                  OpCapability StorageImageWriteWithoutFormat
             %1 = OpExtInstImport "GLSL.std.450"
                  OpMemoryModel Logical GLSL450
                  OpEntryPoint GLCompute %main "main"
                  OpExecutionMode %main LocalSize 1 1 1
                  OpSource GLSL 450
                  OpName %main "main"
                  OpName %img "img"
                  OpDecorate %img DescriptorSet 0
                  OpDecorate %img Binding 0
                  OpDecorate %img NonWritable
          %void = OpTypeVoid
             %3 = OpTypeFunction %void
         %float = OpTypeFloat 32
             %7 = OpTypeImage %float 2D 0 0 0 2 Unknown
%_ptr_UniformConstant_7 = OpTypePointer UniformConstant %7
           %img = OpVariable %_ptr_UniformConstant_7 UniformConstant
           %int = OpTypeInt 32 1
         %v2int = OpTypeVector %int 2
         %int_0 = OpConstant %int 0
            %14 = OpConstantComposite %v2int %int_0 %int_0
       %v4float = OpTypeVector %float 4
       %float_0 = OpConstant %float 0
            %17 = OpConstantComposite %v4float %float_0 %float_0 %float_0 %float_0
          %uint = OpTypeInt 32 0
        %v3uint = OpTypeVector %uint 3
        %uint_1 = OpConstant %uint 1
          %main = OpFunction %void None %3
             %5 = OpLabel
            %10 = OpLoad %7 %img
                  OpImageWrite %10 %14 %17
                  OpReturn
                  OpFunctionEnd
                  )";

    OneOffDescriptorSet ds(m_device, {
                                         {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                                     });

    CreateComputePipelineHelper cs_pipeline(*this);
    cs_pipeline.InitInfo();
    cs_pipeline.cs_.reset(new VkShaderObj(this, csSource, VK_SHADER_STAGE_COMPUTE_BIT, SPV_ENV_VULKAN_1_0, SPV_SOURCE_ASM));
    cs_pipeline.InitState();
    cs_pipeline.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&ds.layout_});
    cs_pipeline.LateBindPipelineInfo();
    cs_pipeline.cp_ci_.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;  // override with wrong value
    cs_pipeline.CreateComputePipeline(true, false);                // need false to prevent late binding

    // Messing with format support, make sure device will handle the image combination
    VkImageFormatProperties format_props;
    if (VK_SUCCESS != vk::GetPhysicalDeviceImageFormatProperties(gpu(), image_format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                                                 VK_IMAGE_USAGE_STORAGE_BIT, VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT,
                                                                 &format_props)) {
        GTEST_SKIP() << "Device will not be able to initialize buffer view skipped";
    }

    auto image_create_info = LvlInitStruct<VkImageCreateInfo>();
    image_create_info.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.format = image_format;
    image_create_info.extent = {32, 32, 1};
    image_create_info.mipLevels = 1;
    image_create_info.arrayLayers = 1;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageObj image(m_device);
    image.Init(image_create_info);

    VkDescriptorImageInfo image_info = {};
    image_info.imageView = image.targetView(image_view_format);
    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = ds.set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptor_write.pImageInfo = &image_info;
    vk::UpdateDescriptorSets(m_device->device(), 1, &descriptor_write, 0, NULL);

    m_commandBuffer->reset();
    m_commandBuffer->begin();

    VkImageMemoryBarrier img_barrier = LvlInitStruct<VkImageMemoryBarrier>();
    img_barrier.srcAccessMask = VK_ACCESS_HOST_READ_BIT;
    img_barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    img_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    img_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    img_barrier.image = image.handle();  // Image mis-matches with FB image
    img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    img_barrier.subresourceRange.baseArrayLayer = 0;
    img_barrier.subresourceRange.baseMipLevel = 0;
    img_barrier.subresourceRange.layerCount = 1;
    img_barrier.subresourceRange.levelCount = 1;
    vk::CmdPipelineBarrier(m_commandBuffer->handle(), VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0,
                           nullptr, 0, nullptr, 1, &img_barrier);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, cs_pipeline.pipeline_);
    vk::CmdBindDescriptorSets(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, cs_pipeline.pipeline_layout_.handle(), 0,
                              1, &ds.set_, 0, nullptr);
    vk::CmdDispatch(m_commandBuffer->handle(), 1, 1, 1);
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, CreateGraphicsPipelineRasterizationOrderAttachmentAccessFlags) {
    TEST_DESCRIPTION("Test for a creating a pipeline with VK_ARM_rasterization_order_attachment_access enabled");

    SetTargetApiVersion(VK_API_VERSION_1_2);
    AddRequiredExtensions(VK_ARM_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto rasterization_order_features = LvlInitStruct<VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesARM>();
    GetPhysicalDeviceFeatures2(rasterization_order_features);

    if (!rasterization_order_features.rasterizationOrderColorAttachmentAccess &&
        !rasterization_order_features.rasterizationOrderDepthAttachmentAccess &&
        !rasterization_order_features.rasterizationOrderStencilAttachmentAccess) {
        GTEST_SKIP() << "Test requires (unsupported) rasterizationOrder*AttachmentAccess";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &rasterization_order_features));

    auto ds_ci = LvlInitStruct<VkPipelineDepthStencilStateCreateInfo>();
    VkPipelineColorBlendAttachmentState cb_as = {};
    auto cb_ci = LvlInitStruct<VkPipelineColorBlendStateCreateInfo>();
    cb_ci.attachmentCount = 1;
    cb_ci.pAttachments = &cb_as;
    VkRenderPass render_pass_handle = VK_NULL_HANDLE;

    auto create_render_pass = [&](VkPipelineDepthStencilStateCreateFlags subpass_flags, vk_testing::RenderPass &render_pass) {
        VkAttachmentDescription attachments[2] = {};
        attachments[0].flags = 0;
        attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
        attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        attachments[1].flags = 0;
        attachments[1].format = FindSupportedDepthStencilFormat(this->gpu());
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference cAttachRef = {};
        cAttachRef.attachment = 0;
        cAttachRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference dsAttachRef = {};
        dsAttachRef.attachment = 1;
        dsAttachRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &cAttachRef;
        subpass.pDepthStencilAttachment = &dsAttachRef;
        subpass.flags = subpass_flags;

        VkRenderPassCreateInfo rpci = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        rpci.attachmentCount = 2;
        rpci.pAttachments = attachments;
        rpci.subpassCount = 1;
        rpci.pSubpasses = &subpass;

        render_pass.init(*this->m_device, rpci);
    };

    auto set_flgas_pipeline_createinfo = [&](CreatePipelineHelper &helper) {
        helper.gp_ci_.pDepthStencilState = &ds_ci;
        helper.gp_ci_.pColorBlendState = &cb_ci;
        helper.gp_ci_.renderPass = render_pass_handle;
    };

    // Color attachment
    if (rasterization_order_features.rasterizationOrderColorAttachmentAccess) {
        cb_ci.flags = VK_PIPELINE_COLOR_BLEND_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_BIT_ARM;
        ds_ci.flags = 0;

        vk_testing::RenderPass render_pass;
        create_render_pass(VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_COLOR_ACCESS_BIT_ARM, render_pass);
        render_pass_handle = render_pass.handle();
        CreatePipelineHelper::OneshotTest(*this, set_flgas_pipeline_createinfo, kErrorBit);
    }

    // Depth attachment
    if (rasterization_order_features.rasterizationOrderDepthAttachmentAccess) {
        cb_ci.flags = 0;
        ds_ci.flags = VK_PIPELINE_DEPTH_STENCIL_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_ARM;

        vk_testing::RenderPass render_pass;
        create_render_pass(VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_DEPTH_ACCESS_BIT_ARM, render_pass);
        render_pass_handle = render_pass.handle();
        CreatePipelineHelper::OneshotTest(*this, set_flgas_pipeline_createinfo, kErrorBit);
    }

    // Stencil attachment
    if (rasterization_order_features.rasterizationOrderStencilAttachmentAccess) {
        cb_ci.flags = 0;
        ds_ci.flags = VK_PIPELINE_DEPTH_STENCIL_STATE_CREATE_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_ARM;

        vk_testing::RenderPass render_pass;
        create_render_pass(VK_SUBPASS_DESCRIPTION_RASTERIZATION_ORDER_ATTACHMENT_STENCIL_ACCESS_BIT_ARM, render_pass);
        render_pass_handle = render_pass.handle();

        CreatePipelineHelper::OneshotTest(*this, set_flgas_pipeline_createinfo, kErrorBit);
    }
}

TEST_F(VkPositiveLayerTest, AttachmentsDisableRasterization) {
    TEST_DESCRIPTION(
        "Create a pipeline with rasterization disabled, containing a valid pColorBlendState and color attachments, but a fragment "
        "shader that does not have any outputs");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char *fs_src = R"glsl(
        #version 450
        void main(){ }
    )glsl";

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.rs_state_ci_.rasterizerDiscardEnable = VK_TRUE;
    pipe.fs_ = std::make_unique<VkShaderObj>(this, fs_src, VK_SHADER_STAGE_FRAGMENT_BIT);
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), pipe.fs_->GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, TestShaderInputOutputMatch) {
    TEST_DESCRIPTION("Test matching vertex shader output with fragment shader input.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char vsSource[] = R"glsl(#version 450

        layout(location = 0) in vec4 dEQP_Position;
        layout(location = 1) in mat3 in0;
        layout(location = 0) out vec4 v1;
        layout(location = 1) out vec4 v2;
        layout(location = 2) out vec4 v3;
        layout(location = 3) out vec4 v4;

        void main() {
            v1 = mat4(in0)[0];
            v2 = mat4(in0)[1];
            v3 = mat4(in0)[2];
            v4 = mat4(in0)[3];
            gl_Position = dEQP_Position;
        }
    )glsl";

    const char fsSource[] = R"glsl(#version 450

        bool isOk (mat4 a, mat4 b, float eps) {
            vec4 diff = max(max(abs(a[0]-b[0]), abs(a[1]-b[1])), max(abs(a[2]-b[2]), abs(a[3]-b[3])));
            return all(lessThanEqual(diff, vec4(eps)));
        }

        layout(location = 0) in mat4 out0;
        layout(set = 0, binding = 0) uniform block { mat4 ref_out0; };
        layout(location = 0) out vec4 color;

        void main() {
            bool RES = isOk(out0, ref_out0, 0.05);
            color = vec4(RES, RES, RES, 1.0);
        }
    )glsl";
    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkVertexInputBindingDescription vertex_input_binding_description{};
    vertex_input_binding_description.binding = 0;
    vertex_input_binding_description.stride = 0;
    vertex_input_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vertex_input_attribute_descriptions[4];
    vertex_input_attribute_descriptions[0].location = 0;
    vertex_input_attribute_descriptions[0].binding = 0;
    vertex_input_attribute_descriptions[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertex_input_attribute_descriptions[0].offset = 0;
    vertex_input_attribute_descriptions[1].location = 1;
    vertex_input_attribute_descriptions[1].binding = 0;
    vertex_input_attribute_descriptions[1].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertex_input_attribute_descriptions[1].offset = 32;
    vertex_input_attribute_descriptions[2].location = 2;
    vertex_input_attribute_descriptions[2].binding = 0;
    vertex_input_attribute_descriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertex_input_attribute_descriptions[2].offset = 64;
    vertex_input_attribute_descriptions[3].location = 3;
    vertex_input_attribute_descriptions[3].binding = 0;
    vertex_input_attribute_descriptions[3].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertex_input_attribute_descriptions[3].offset = 96;

    OneOffDescriptorSet ds(
        m_device, {
                      {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
                  });

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.vi_ci_.vertexBindingDescriptionCount = 1;
    pipe.vi_ci_.pVertexBindingDescriptions = &vertex_input_binding_description;
    pipe.vi_ci_.vertexAttributeDescriptionCount = 4;
    pipe.vi_ci_.pVertexAttributeDescriptions = vertex_input_attribute_descriptions;
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&ds.layout_});
    pipe.CreateGraphicsPipeline();

    VkBufferObj uniform_buffer;
    auto ub_ci = LvlInitStruct<VkBufferCreateInfo>();
    ub_ci.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    ub_ci.size = 1024;
    uniform_buffer.init(*m_device, ub_ci);
    ds.WriteDescriptorBufferInfo(0, uniform_buffer.handle(), 0, 1024);
    ds.UpdateDescriptorSets();

    VkBufferObj buffer;
    VkBufferCreateInfo vb_ci = LvlInitStruct<VkBufferCreateInfo>();
    vb_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    vb_ci.size = 1024;
    buffer.init(*m_device, vb_ci);
    VkBuffer buffer_handle = buffer.handle();
    VkDeviceSize offset = 0;

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);

    vk::CmdBindVertexBuffers(m_commandBuffer->handle(), 0, 1, &buffer_handle, &offset);
    vk::CmdBindDescriptorSets(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_layout_.handle(), 0, 1,
                              &ds.set_, 0, nullptr);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    vk::CmdDraw(m_commandBuffer->handle(), 3, 1, 0, 0);

    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
}

// Spec doesn't clarify if this is valid or not
// https://gitlab.khronos.org/vulkan/vulkan/-/issues/3445
TEST_F(VkPositiveLayerTest, DISABLED_TestShaderInputOutputMatch2) {
    TEST_DESCRIPTION("Test matching vertex shader output with fragment shader input.");

    ASSERT_NO_FATAL_FAILURE(Init());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    const char vsSource[] = R"glsl(#version 450
        layout(location = 0) out vec2 v1;
        layout(location = 1) out vec2 v2;
        layout(location = 2) out vec2 v3;

        void main() {
            v1 = vec2(0.0f);
            v2 = vec2(1.0f);
            v3 = vec2(0.5f);
        }
    )glsl";

    const char fsSource[] = R"glsl(#version 450
        layout(location = 0) in mat3x2 v;
        layout(location = 0) out vec4 color;

        void main() {
            color = vec4(v[0][0], v[0][1], v[1][0], v[1][1]);
        }
    )glsl";
    VkShaderObj vs(this, vsSource, VK_SHADER_STAGE_VERTEX_BIT);
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {vs.GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();
}

TEST_F(VkPositiveLayerTest, TestDualBlendShader) {
    TEST_DESCRIPTION("Test drawing with dual source blending with too many fragment output attachments.");

    SetTargetApiVersion(VK_API_VERSION_1_1);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }

    auto features2 = LvlInitStruct<VkPhysicalDeviceFeatures2>();
    GetPhysicalDeviceFeatures2(features2);

    if (features2.features.dualSrcBlend == VK_FALSE) {
        GTEST_SKIP() << "dualSrcBlend feature is not available";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    char const *fsSource = R"glsl(
        #version 450
        layout(location = 0, index = 0) out vec4 c1;
        layout(location = 0, index = 1) out vec4 c2;
        void main(){
            c1 = vec4(0.5f);
            c2 = vec4(0.5f);
        }
    )glsl";

    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    VkPipelineColorBlendAttachmentState cb_attachments = {};
    cb_attachments.blendEnable = VK_TRUE;
    cb_attachments.srcColorBlendFactor = VK_BLEND_FACTOR_SRC1_COLOR;  // bad!
    cb_attachments.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
    cb_attachments.colorBlendOp = VK_BLEND_OP_ADD;
    cb_attachments.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    cb_attachments.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    cb_attachments.alphaBlendOp = VK_BLEND_OP_ADD;

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.cb_attachments_[0] = cb_attachments;
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.CreateGraphicsPipeline();

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);

    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);

    vk::CmdDraw(m_commandBuffer->handle(), 3, 1, 0, 0);

    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();
}

TEST_F(VkPositiveLayerTest, TestUpdateAfterBind) {
    TEST_DESCRIPTION("Test UPDATE_AFTER_BIND does not reset command buffers.");

    SetTargetApiVersion(VK_API_VERSION_1_1);
    AddRequiredExtensions(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto descriptor_indexing = LvlInitStruct<VkPhysicalDeviceDescriptorIndexingFeatures>();
    auto synchronization2 = LvlInitStruct<VkPhysicalDeviceSynchronization2FeaturesKHR>(&descriptor_indexing);
    auto features2 = GetPhysicalDeviceFeatures2(synchronization2);
    if (descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE) {
        GTEST_SKIP() << "descriptorBindingStorageBufferUpdateAfterBind feature is not available";
    }
    if (synchronization2.synchronization2 == VK_FALSE) {
        GTEST_SKIP() << "synchronization2 feature is not available";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    auto vkQueueSubmit2KHR =
        reinterpret_cast<PFN_vkQueueSubmit2KHR>(vk::GetDeviceProcAddr(m_device->device(), "vkQueueSubmit2KHR"));
    assert(vkQueueSubmit2KHR != nullptr);

    auto buffer_ci = LvlInitStruct<VkBufferCreateInfo>();
    buffer_ci.size = 4096;
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkBuffer buffer1, buffer2, buffer3;
    vk::CreateBuffer(device(), &buffer_ci, nullptr, &buffer1);
    vk::CreateBuffer(device(), &buffer_ci, nullptr, &buffer2);
    vk::CreateBuffer(device(), &buffer_ci, nullptr, &buffer3);

    VkMemoryRequirements buffer_mem_reqs;
    vk::GetBufferMemoryRequirements(device(), buffer1, &buffer_mem_reqs);

    auto alloc_info = LvlInitStruct<VkMemoryAllocateInfo>();
    alloc_info.allocationSize = buffer_mem_reqs.size;

    VkDeviceMemory memory1, memory2, memory3;
    vk::AllocateMemory(device(), &alloc_info, nullptr, &memory1);
    vk::AllocateMemory(device(), &alloc_info, nullptr, &memory2);
    vk::AllocateMemory(device(), &alloc_info, nullptr, &memory3);

    vk::BindBufferMemory(device(), buffer1, memory1, 0);
    vk::BindBufferMemory(device(), buffer2, memory2, 0);
    vk::BindBufferMemory(device(), buffer3, memory3, 0);

    OneOffDescriptorSet::Bindings binding_defs = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
    };
    VkDescriptorBindingFlagsEXT flags[2] = {VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT, 0};
    auto flags_create_info = LvlInitStruct<VkDescriptorSetLayoutBindingFlagsCreateInfoEXT>();
    flags_create_info.bindingCount = 2;
    flags_create_info.pBindingFlags = flags;
    OneOffDescriptorSet descriptor_set(m_device, binding_defs, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
                                       &flags_create_info, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT);
    const VkPipelineLayoutObj pipeline_layout(m_device, {&descriptor_set.layout_});

    VkDescriptorBufferInfo buffer_info = {buffer1, 0, sizeof(uint32_t)};

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = descriptor_set.set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_write.pBufferInfo = &buffer_info;

    vk::UpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);

    descriptor_write.dstBinding = 1;
    buffer_info.buffer = buffer3;
    vk::UpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);
    descriptor_write.dstBinding = 0;

    const char fsSource[] = R"glsl(
        #version 450
        layout (set = 0, binding = 0) buffer buf1 {
            float a;
        } ubuf1;
        layout (set = 0, binding = 1) buffer buf2 {
            float a;
        } ubuf2;
        void main() {
           float f = ubuf1.a * ubuf2.a;
        }
    )glsl";
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&descriptor_set.layout_});
    pipe.CreateGraphicsPipeline();

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    vk::CmdBindDescriptorSets(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout.handle(), 0, 1,
                              &descriptor_set.set_, 0, nullptr);
    vk::CmdDraw(m_commandBuffer->handle(), 3, 1, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    vk::DestroyBuffer(device(), buffer1, nullptr);
    buffer_info.buffer = buffer2;
    vk::UpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);

    auto cb_info = LvlInitStruct<VkCommandBufferSubmitInfoKHR>();
    cb_info.commandBuffer = m_commandBuffer->handle();

    auto submit_info = LvlInitStruct<VkSubmitInfo2KHR>();
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &cb_info;

    vkQueueSubmit2KHR(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vk::QueueWaitIdle(m_device->m_queue);

    vk::DestroyBuffer(device(), buffer2, nullptr);
    vk::DestroyBuffer(device(), buffer3, nullptr);

    vk::FreeMemory(device(), memory1, nullptr);
    vk::FreeMemory(device(), memory2, nullptr);
    vk::FreeMemory(device(), memory3, nullptr);
}

TEST_F(VkPositiveLayerTest, TestPartiallyBoundDescriptors) {
    TEST_DESCRIPTION("Test partially bound descriptors do not reset command buffers.");

    SetTargetApiVersion(VK_API_VERSION_1_1);
    AddRequiredExtensions(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    AddRequiredExtensions(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (DeviceValidationVersion() < VK_API_VERSION_1_1) {
        GTEST_SKIP() << "At least Vulkan version 1.1 is required";
    }
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto descriptor_indexing = LvlInitStruct<VkPhysicalDeviceDescriptorIndexingFeatures>();
    auto synchronization2 = LvlInitStruct<VkPhysicalDeviceSynchronization2FeaturesKHR>(&descriptor_indexing);
    auto features2 = GetPhysicalDeviceFeatures2(synchronization2);
    if (descriptor_indexing.descriptorBindingStorageBufferUpdateAfterBind == VK_FALSE) {
        GTEST_SKIP() << "descriptorBindingStorageBufferUpdateAfterBind feature is not available";
    }
    if (synchronization2.synchronization2 == VK_FALSE) {
        GTEST_SKIP() << "synchronization2 feature is not available";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    auto vkQueueSubmit2KHR =
        reinterpret_cast<PFN_vkQueueSubmit2KHR>(vk::GetDeviceProcAddr(m_device->device(), "vkQueueSubmit2KHR"));
    assert(vkQueueSubmit2KHR != nullptr);

    auto buffer_ci = LvlInitStruct<VkBufferCreateInfo>();
    buffer_ci.size = 4096;
    buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    VkBuffer buffer1, buffer3;
    vk::CreateBuffer(device(), &buffer_ci, nullptr, &buffer1);
    vk::CreateBuffer(device(), &buffer_ci, nullptr, &buffer3);

    VkMemoryRequirements buffer_mem_reqs;
    vk::GetBufferMemoryRequirements(device(), buffer1, &buffer_mem_reqs);

    auto alloc_info = LvlInitStruct<VkMemoryAllocateInfo>();
    alloc_info.allocationSize = buffer_mem_reqs.size;

    VkDeviceMemory memory1, memory3;
    vk::AllocateMemory(device(), &alloc_info, nullptr, &memory1);
    vk::AllocateMemory(device(), &alloc_info, nullptr, &memory3);

    vk::BindBufferMemory(device(), buffer1, memory1, 0);
    vk::BindBufferMemory(device(), buffer3, memory3, 0);

    OneOffDescriptorSet::Bindings binding_defs = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_ALL, nullptr},
    };
    VkDescriptorBindingFlagsEXT flags[2] = {VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, 0};
    auto flags_create_info = LvlInitStruct<VkDescriptorSetLayoutBindingFlagsCreateInfoEXT>();
    flags_create_info.bindingCount = 2;
    flags_create_info.pBindingFlags = flags;
    OneOffDescriptorSet descriptor_set(m_device, binding_defs, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
                                       &flags_create_info, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT);
    const VkPipelineLayoutObj pipeline_layout(m_device, {&descriptor_set.layout_});

    VkDescriptorBufferInfo buffer_info = {buffer1, 0, sizeof(uint32_t)};

    VkWriteDescriptorSet descriptor_write = LvlInitStruct<VkWriteDescriptorSet>();
    descriptor_write.dstSet = descriptor_set.set_;
    descriptor_write.dstBinding = 0;
    descriptor_write.descriptorCount = 1;
    descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptor_write.pBufferInfo = &buffer_info;

    vk::UpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);

    descriptor_write.dstBinding = 1;
    buffer_info.buffer = buffer3;
    vk::UpdateDescriptorSets(device(), 1, &descriptor_write, 0, nullptr);
    descriptor_write.dstBinding = 0;

    const char fsSource[] = R"glsl(
        #version 450
        layout (set = 0, binding = 0) buffer buf1 {
            float a;
        } ubuf1;
        layout (set = 0, binding = 1) buffer buf2 {
            float a;
        } ubuf2;
        void main() {
           float f = ubuf2.a;
        }
    )glsl";
    VkShaderObj fs(this, fsSource, VK_SHADER_STAGE_FRAGMENT_BIT);

    CreatePipelineHelper pipe(*this);
    pipe.InitInfo();
    pipe.shader_stages_ = {pipe.vs_->GetStageCreateInfo(), fs.GetStageCreateInfo()};
    pipe.InitState();
    pipe.pipeline_layout_ = VkPipelineLayoutObj(m_device, {&descriptor_set.layout_});
    pipe.CreateGraphicsPipeline();

    m_commandBuffer->begin();
    m_commandBuffer->BeginRenderPass(m_renderPassBeginInfo);
    vk::CmdBindPipeline(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipe.pipeline_);
    vk::CmdBindDescriptorSets(m_commandBuffer->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout.handle(), 0, 1,
                              &descriptor_set.set_, 0, nullptr);
    vk::CmdDraw(m_commandBuffer->handle(), 3, 1, 0, 0);
    m_commandBuffer->EndRenderPass();
    m_commandBuffer->end();

    vk::DestroyBuffer(device(), buffer1, nullptr);

    auto cb_info = LvlInitStruct<VkCommandBufferSubmitInfoKHR>();
    cb_info.commandBuffer = m_commandBuffer->handle();

    auto submit_info = LvlInitStruct<VkSubmitInfo2KHR>();
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &cb_info;

    vkQueueSubmit2KHR(m_device->m_queue, 1, &submit_info, VK_NULL_HANDLE);
    vk::QueueWaitIdle(m_device->m_queue);

    vk::DestroyBuffer(device(), buffer3, nullptr);

    vk::FreeMemory(device(), memory1, nullptr);
    vk::FreeMemory(device(), memory3, nullptr);
}

TEST_F(VkPositiveLayerTest, ShaderZeroInitializeWorkgroupMemoryFeature) {
    TEST_DESCRIPTION("Enable and use shaderZeroInitializeWorkgroupMemory feature");

    AddRequiredExtensions(VK_KHR_ZERO_INITIALIZE_WORKGROUP_MEMORY_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));

    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " required but not supported";
    }

    auto zero_initialize_work_group_memory_features = LvlInitStruct<VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR>();
    auto features2 = GetPhysicalDeviceFeatures2(zero_initialize_work_group_memory_features);
    if (!zero_initialize_work_group_memory_features.shaderZeroInitializeWorkgroupMemory) {
        GTEST_SKIP() << "VkPhysicalDeviceZeroInitializeWorkgroupMemoryFeaturesKHR::shaderZeroInitializeWorkgroupMemory is required but not enabled.";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &features2));

    const char *spv_source = R"(
               OpCapability Shader
          %1 = OpExtInstImport "GLSL.std.450"
               OpMemoryModel Logical GLSL450
               OpEntryPoint GLCompute %main "main"
               OpExecutionMode %main LocalSize 1 1 1
               OpSource GLSL 450
               OpName %main "main"
               OpName %counter "counter"
       %void = OpTypeVoid
          %3 = OpTypeFunction %void
       %uint = OpTypeInt 32 0
%_ptr_Workgroup_uint = OpTypePointer Workgroup %uint
  %zero_uint = OpConstantNull %uint
    %counter = OpVariable %_ptr_Workgroup_uint Workgroup %zero_uint
       %main = OpFunction %void None %3
          %5 = OpLabel
               OpReturn
               OpFunctionEnd
        )";

    auto cs = VkShaderObj::CreateFromASM(*this, VK_SHADER_STAGE_COMPUTE_BIT, spv_source, "main", nullptr);
    const auto set_info = [&cs](CreateComputePipelineHelper &helper) { helper.cs_ = std::move(cs); };
    CreateComputePipelineHelper::OneshotTest(*this, set_info, kErrorBit);
}

// TODO: CTS was written, but still fails on many older drivers in CI
// https://gitlab.khronos.org/Tracker/vk-gl-cts/-/issues/3736
TEST_F(VkPositiveLayerTest, DISABLED_GraphicsPipelineStageCreationFeedbackCount0) {
    TEST_DESCRIPTION("Test graphics pipeline feedback stage count check with 0.");

    AddRequiredExtensions(VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME);
    // need for IsDriver check
    AddRequiredExtensions(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << VK_EXT_PIPELINE_CREATION_FEEDBACK_EXTENSION_NAME << " not supported";
    }
    // This test hits a bug in the driver, CTS was written, but incase using an old driver
    if (IsDriver(VK_DRIVER_ID_NVIDIA_PROPRIETARY)) {
        GTEST_SKIP() << "This test should not be run on the NVIDIA proprietary driver.";
    }
    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    auto feedback_info = LvlInitStruct<VkPipelineCreationFeedbackCreateInfoEXT>();
    VkPipelineCreationFeedbackEXT feedbacks[1] = {};
    // Set flags to known value that the driver has to overwrite
    feedbacks[0].flags = VK_PIPELINE_CREATION_FEEDBACK_FLAG_BITS_MAX_ENUM;

    feedback_info.pPipelineCreationFeedback = &feedbacks[0];
    feedback_info.pipelineStageCreationFeedbackCount = 0;

    auto set_feedback = [&feedback_info](CreatePipelineHelper &helper) { helper.gp_ci_.pNext = &feedback_info; };

    CreatePipelineHelper::OneshotTest(*this, set_feedback, kErrorBit);
}

TEST_F(VkPositiveLayerTest, ShaderModuleIdentifierGPL) {
    TEST_DESCRIPTION("Create pipeline sub-state that references shader module identifiers");
    AddRequiredExtensions(VK_EXT_SHADER_MODULE_IDENTIFIER_EXTENSION_NAME);
    AddRequiredExtensions(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    auto gpl_features = LvlInitStruct<VkPhysicalDeviceGraphicsPipelineLibraryFeaturesEXT>();
    auto pipeline_cache_control_features = LvlInitStruct<VkPhysicalDevicePipelineCreationCacheControlFeatures>(&gpl_features);
    auto shader_module_id_features =
        LvlInitStruct<VkPhysicalDeviceShaderModuleIdentifierFeaturesEXT>(&pipeline_cache_control_features);
    GetPhysicalDeviceFeatures2(shader_module_id_features);

    if (!gpl_features.graphicsPipelineLibrary) {
        GTEST_SKIP() << "graphicsPipelineLibrary feature not supported";
    }
    if (!shader_module_id_features.shaderModuleIdentifier) {
        GTEST_SKIP() << "shaderModuleIdentifier feature not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState(nullptr, &shader_module_id_features));
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    // Create a pre-raster pipeline referencing a VS via identifier, with the VS identifier queried from a shader module
    VkShaderObj vs(this, bindStateVertShaderText, VK_SHADER_STAGE_VERTEX_BIT);
    ASSERT_TRUE(vs.initialized());

    auto vkGetShaderModuleIdentifierEXT = reinterpret_cast<PFN_vkGetShaderModuleIdentifierEXT>(
        vk::GetDeviceProcAddr(m_device->device(), "vkGetShaderModuleIdentifierEXT"));
    ASSERT_NE(vkGetShaderModuleIdentifierEXT, nullptr);

    auto vs_identifier = LvlInitStruct<VkShaderModuleIdentifierEXT>();
    vkGetShaderModuleIdentifierEXT(device(), vs.handle(), &vs_identifier);

    auto sm_id_create_info = LvlInitStruct<VkPipelineShaderStageModuleIdentifierCreateInfoEXT>();
    sm_id_create_info.identifierSize = vs_identifier.identifierSize;
    sm_id_create_info.pIdentifier = vs_identifier.identifier;

    auto stage_ci = LvlInitStruct<VkPipelineShaderStageCreateInfo>(&sm_id_create_info);
    stage_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage_ci.module = VK_NULL_HANDLE;
    stage_ci.pName = "main";

    CreatePipelineHelper pipe(*this);
    pipe.InitPreRasterLibInfo(1, &stage_ci);
    pipe.gp_ci_.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
    pipe.InitState();
    ASSERT_VK_SUCCESS(pipe.CreateGraphicsPipeline());

    // Create a fragment shader library with FS referencing an identifier queried from VkShaderModuleCreateInfo
    const auto fs_spv = GLSLToSPV(VK_SHADER_STAGE_FRAGMENT_BIT, bindStateFragShaderText);
    auto fs_ci = LvlInitStruct<VkShaderModuleCreateInfo>();
    fs_ci.codeSize = fs_spv.size() * sizeof(decltype(fs_spv)::value_type);
    fs_ci.pCode = fs_spv.data();

    auto vkGetShaderModuleCreateInfoIdentifierEXT = reinterpret_cast<PFN_vkGetShaderModuleCreateInfoIdentifierEXT>(
        vk::GetDeviceProcAddr(m_device->device(), "vkGetShaderModuleCreateInfoIdentifierEXT"));
    ASSERT_NE(vkGetShaderModuleCreateInfoIdentifierEXT, nullptr);

    auto fs_identifier = LvlInitStruct<VkShaderModuleIdentifierEXT>();
    vkGetShaderModuleCreateInfoIdentifierEXT(device(), &fs_ci, &fs_identifier);

    sm_id_create_info.identifierSize = fs_identifier.identifierSize;
    sm_id_create_info.pIdentifier = fs_identifier.identifier;

    auto fs_stage_ci = LvlInitStruct<VkPipelineShaderStageCreateInfo>(&sm_id_create_info);
    fs_stage_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fs_stage_ci.module = VK_NULL_HANDLE;
    fs_stage_ci.pName = "main";

    CreatePipelineHelper fs_pipe(*this);
    fs_pipe.InitFragmentLibInfo(1, &fs_stage_ci);
    fs_pipe.gp_ci_.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
    fs_pipe.gp_ci_.layout = pipe.gp_ci_.layout;
    fs_pipe.InitState();
    ASSERT_VK_SUCCESS(fs_pipe.CreateGraphicsPipeline(true, false));

    // Create a complete pipeline with the above pre-raster fs libraries
    CreatePipelineHelper vi_pipe(*this);
    vi_pipe.InitVertexInputLibInfo();
    vi_pipe.CreateGraphicsPipeline();

    CreatePipelineHelper fo_pipe(*this);
    fo_pipe.InitFragmentOutputLibInfo();
    fo_pipe.CreateGraphicsPipeline();

    VkPipeline libraries[4] = {
        vi_pipe.pipeline_,
        pipe.pipeline_,
        fs_pipe.pipeline_,
        fo_pipe.pipeline_,
    };
    auto link_info = LvlInitStruct<VkPipelineLibraryCreateInfoKHR>();
    link_info.libraryCount = size(libraries);
    link_info.pLibraries = libraries;

    auto pipe_ci = LvlInitStruct<VkGraphicsPipelineCreateInfo>(&link_info);
    pipe_ci.flags |= VK_PIPELINE_CREATE_FAIL_ON_PIPELINE_COMPILE_REQUIRED_BIT;
    pipe_ci.layout = pipe.gp_ci_.layout;
    vk_testing::Pipeline exe_pipe(*m_device, pipe_ci);
}

TEST_F(VkPositiveLayerTest, ViewportSwizzleNV) {
    AddRequiredExtensions(VK_NV_VIEWPORT_SWIZZLE_EXTENSION_NAME);
    ASSERT_NO_FATAL_FAILURE(InitFramework());
    if (!AreRequiredExtensionsEnabled()) {
        GTEST_SKIP() << RequiredExtensionsNotSupported() << " not supported";
    }

    ASSERT_NO_FATAL_FAILURE(InitState());
    ASSERT_NO_FATAL_FAILURE(InitRenderTarget());

    std::array<VkViewportSwizzleNV, 2> swizzle = {};

    swizzle.fill({VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_X_NV, VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Y_NV,
                  VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_Z_NV, VK_VIEWPORT_COORDINATE_SWIZZLE_POSITIVE_W_NV});

    std::array<VkViewport, 2> viewports = {};
    std::array<VkRect2D, 2> scissors = {};

    viewports.fill({0, 0, 16, 16, 0, 1});
    scissors.fill({{0, 0}, {16, 16}});

    // Test case where VkPipelineViewportSwizzleStateCreateInfoNV::viewportCount is EQUAL TO viewportCount set in
    // VkPipelineViewportStateCreateInfo
    {
        auto vp_swizzle_state = LvlInitStruct<VkPipelineViewportSwizzleStateCreateInfoNV>();
        vp_swizzle_state.viewportCount = size32(viewports);
        vp_swizzle_state.pViewportSwizzles = swizzle.data();

        auto break_vp_count = [&vp_swizzle_state, &viewports, &scissors](CreatePipelineHelper &helper) {
            helper.vp_state_ci_.viewportCount = size32(viewports);
            helper.vp_state_ci_.pViewports = viewports.data();
            helper.vp_state_ci_.scissorCount = size32(scissors);
            helper.vp_state_ci_.pScissors = scissors.data();
            helper.vp_state_ci_.pNext = &vp_swizzle_state;
            ASSERT_TRUE(vp_swizzle_state.viewportCount == helper.vp_state_ci_.viewportCount);
        };

        CreatePipelineHelper::OneshotTest(*this, break_vp_count, kErrorBit);
    }

    // Test case where VkPipelineViewportSwizzleStateCreateInfoNV::viewportCount is GREATER THAN viewportCount set in
    // VkPipelineViewportStateCreateInfo
    {
        auto vp_swizzle_state = LvlInitStruct<VkPipelineViewportSwizzleStateCreateInfoNV>();
        vp_swizzle_state.viewportCount = size32(viewports);
        vp_swizzle_state.pViewportSwizzles = swizzle.data();

        auto break_vp_count = [&vp_swizzle_state, &viewports, &scissors](CreatePipelineHelper &helper) {
            helper.vp_state_ci_.viewportCount = 1;
            helper.vp_state_ci_.pViewports = viewports.data();
            helper.vp_state_ci_.scissorCount = 1;
            helper.vp_state_ci_.pScissors = scissors.data();
            helper.vp_state_ci_.pNext = &vp_swizzle_state;
            ASSERT_TRUE(vp_swizzle_state.viewportCount > helper.vp_state_ci_.viewportCount);
        };

        CreatePipelineHelper::OneshotTest(*this, break_vp_count, kErrorBit);
    }
}
