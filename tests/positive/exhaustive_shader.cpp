/*
 * Copyright (c) 2022 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Author: Spencer Fricke <spencerfricke@gmail.com>
 */

#include "../layer_validation_tests.h"
#include "vk_extension_helper.h"

#include <spirv_reflect.h>

#include <fstream>
#include <vector>
#include <algorithm>

//
// Exhaustive Shader Test
//
// see docs/exhaustive_shader_test.md for more details

#define ASSERT_REFLECT(err)                                              \
    {                                                                    \
        const SpvReflectResult result = err;                             \
        ASSERT_EQ(SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS, result); \
    }

class VkExhaustiveShaderTest : public VkLayerTest {
  public:
    VkExhaustiveShaderTest() { spirv_data.clear(); }

  protected:
    void CreatePipelineLayout();
    void CreateGraphicsPipeline();
    void CreateComputePipeline();

    void GetTypeDescription(SpvReflectTypeDescription& description, SpvReflectFormat format, std::string& source);
    std::string CreateCustomTypePatch(SpvReflectTypeDescription& description);
    void CreatePassThroughVertex(std::string& source);
    void CreatePassThroughTessellationEval(std::string& source);
    void CreatePassThroughTessellationControl(std::string& source);

    SpvReflectShaderModule reflect_module;
    VkShaderStageFlagBits shader_stage;
    std::vector<uint32_t> spirv_data;
    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;

    std::vector<uint32_t> input_atachment_indices;
    std::vector<SpvReflectInterfaceVariable*> input_variables;
    std::vector<SpvReflectInterfaceVariable*> output_variables;

    // Support for features that effect shaders capabilities
    bool support_geometry;
    bool support_tessellation;
    bool support_demoteToHelperInvocation;
    bool support_shaderClock;
    bool support_descriptorIndexing;
    bool support_shaderDrawParameters;
    bool support_16BitStorage;
    bool support_8BitStorage;
    bool support_shaderAtomicInt64;
    bool support_shaderFloat16Int8;
    bool support_shaderSubgroupExtendedTypes;
    bool support_multiview;

};

// Some Builtins like 'gl_in' of tessellation shaders are structs and so the gl_* identifiers are reserved. Can't assume all
// structs are Builtins.
static bool IsBuiltinType(SpvReflectInterfaceVariable* description) {
    return (description->built_in >= 0 || std::string(description->name).find("gl_") == 0);
};

void VkExhaustiveShaderTest::CreatePipelineLayout() {
    // get the VkDescriptorSetLayouts
    std::vector<VkDescriptorSetLayout> layouts;
    {
        uint32_t descriptor_count = 0;
        ASSERT_REFLECT(spvReflectEnumerateDescriptorSets(&reflect_module, &descriptor_count, nullptr));
        std::vector<SpvReflectDescriptorSet*> descriptors(descriptor_count);
        ASSERT_REFLECT(spvReflectEnumerateDescriptorSets(&reflect_module, &descriptor_count, descriptors.data()));

        struct DescriptorSetLayoutData {
            uint32_t set;
            VkDescriptorSetLayoutCreateInfo create_info;
            std::vector<VkDescriptorSetLayoutBinding> bindings;
        };

        std::vector<DescriptorSetLayoutData> set_layouts(descriptors.size(), DescriptorSetLayoutData{});
        for (uint32_t set_id = 0; set_id < descriptors.size(); set_id++) {
            const SpvReflectDescriptorSet& reflect_set = *(descriptors[set_id]);
            DescriptorSetLayoutData& layout = set_layouts[set_id];
            layout.bindings.resize(reflect_set.binding_count);
            for (uint32_t binding_id = 0; binding_id < reflect_set.binding_count; binding_id++) {
                const SpvReflectDescriptorBinding& reflect_binding = *(reflect_set.bindings[binding_id]);
                VkDescriptorSetLayoutBinding& layout_binding = layout.bindings[binding_id];

                layout_binding.descriptorType = static_cast<VkDescriptorType>(reflect_binding.descriptor_type);
                layout_binding.binding = reflect_binding.binding;
                layout_binding.descriptorCount = 1;
                for (uint32_t dim_id = 0; dim_id < reflect_binding.array.dims_count; dim_id++) {
                    layout_binding.descriptorCount *= reflect_binding.array.dims[dim_id];
                }
                layout_binding.stageFlags = shader_stage;

                if (layout_binding.descriptorType == VkDescriptorType::VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
                    input_atachment_indices.push_back(reflect_binding.input_attachment_index);
                }
            }
            layout.set = reflect_set.set;
            layout.create_info = LvlInitStruct<VkDescriptorSetLayoutCreateInfo>();
            layout.create_info.bindingCount = reflect_set.binding_count;
            layout.create_info.pBindings = layout.bindings.data();
        }

        uint32_t id = 0;
        for (DescriptorSetLayoutData& set_layout : set_layouts) {
            while (set_layout.set > id) {
                id++;
                VkDescriptorSetLayout layout = VK_NULL_HANDLE;
                ASSERT_VK_SUCCESS(vk::CreateDescriptorSetLayout(device(), &set_layout.create_info, nullptr, &layout));
                layouts.push_back(layout);
            }
            VkDescriptorSetLayout layout = VK_NULL_HANDLE;
            ASSERT_VK_SUCCESS(vk::CreateDescriptorSetLayout(device(), &set_layout.create_info, nullptr, &layout));
            layouts.push_back(layout);
            id++;
        }
    }

    // Get the push constants
    std::vector<VkPushConstantRange> push_constants;
    {
        uint32_t count = 0;
        ASSERT_REFLECT(spvReflectEnumeratePushConstantBlocks(&reflect_module, &count, nullptr));
        std::vector<SpvReflectBlockVariable*> constants(count);
        ASSERT_REFLECT(spvReflectEnumeratePushConstantBlocks(&reflect_module, &count, constants.data()));

        for (uint32_t i = 0; i < count; i++) {
            VkPushConstantRange range = {};
            range.stageFlags = static_cast<VkShaderStageFlagBits>(reflect_module.shader_stage);
            range.size = constants[i]->size;
            range.offset = constants[i]->offset;
            push_constants.push_back(range);
        }
    }

    // Create the pipeline layout
    {
        VkPipelineLayoutCreateInfo info = LvlInitStruct<VkPipelineLayoutCreateInfo>();
        info.pSetLayouts = layouts.data();
        info.setLayoutCount = static_cast<uint32_t>(layouts.size());
        info.pPushConstantRanges = push_constants.data();
        info.pushConstantRangeCount = static_cast<uint32_t>(push_constants.size());
        ASSERT_VK_SUCCESS(vk::CreatePipelineLayout(device(), &info, nullptr, &pipeline_layout));
    }

    // cleanup
    for (VkDescriptorSetLayout& layout : layouts) {
        vk::DestroyDescriptorSetLayout(device(), layout, nullptr);
    }
}

void VkExhaustiveShaderTest::GetTypeDescription(SpvReflectTypeDescription& description, SpvReflectFormat format,
                                                std::string& source) {
    // Has a predefined type (probably struct)
    if (description.type_name != nullptr) {
        source += description.type_name;
        return;
    }

    if (description.op == SpvOp::SpvOpTypeArray) {
        // An array input has a <type> output from the shader
        // SPIRV Reflect does not store the type of the array so the type
        // must be determined by the description
        if ((description.traits.numeric.matrix.column_count > 0) && (description.traits.numeric.matrix.row_count > 0)) {
            description.op = SpvOp::SpvOpTypeMatrix;
        } else if (description.traits.numeric.vector.component_count > 0) {
            description.op = SpvOp::SpvOpTypeVector;
        } else {
            // either a float, bool, or int. Must be inferred from format
            switch (format) {
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32_SFLOAT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32_SFLOAT:
                    description.op = SpvOp::SpvOpTypeFloat;
                    break;
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32_SINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32_SINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32_SINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32B32_UINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32G32_UINT:
                case SpvReflectFormat::SPV_REFLECT_FORMAT_R32_UINT:
                    description.op = SpvOp::SpvOpTypeInt;
                    break;
                default:
                    ASSERT_FALSE(true) << "undefined format";
                    break;
            }
        }
    }

    switch (description.op) {
        case SpvOp::SpvOpTypeBool: {
            source += "bool";
            break;
        }
        case SpvOp::SpvOpTypeFloat: {
            source += "float";
            break;
        }
        case SpvOp::SpvOpTypeInt: {
            if (description.traits.numeric.scalar.signedness == 0) {
                source += "u";
            }
            source += "int";
            break;
        }
        case SpvOp::SpvOpTypeVector: {
            source += "vec";
            const uint32_t component_count = description.traits.numeric.vector.component_count;
            source += std::to_string(component_count);
            break;
        }
        case SpvOp::SpvOpTypeMatrix: {
            source += "mat";
            const uint32_t column_count = description.traits.numeric.matrix.column_count;
            const uint32_t row_count = description.traits.numeric.matrix.row_count;
            if (column_count == row_count) {
                source += std::to_string(column_count);
            } else {
                source += std::to_string(column_count) + "x" + std::to_string(row_count);
            }
            break;
        }
        default:
            ASSERT_FALSE(true) << "Unsupported type found";
            break;
    }
}

std::string VkExhaustiveShaderTest::CreateCustomTypePatch(SpvReflectTypeDescription& description) {
    // Only custom type allowed in GLSL is struct
    std::string patch = "struct ";
    patch += description.type_name;
    patch += "\n{\n";
    for (uint32_t i = 0; i < description.member_count; i++) {
        GetTypeDescription(description.members[i], SpvReflectFormat::SPV_REFLECT_FORMAT_UNDEFINED, patch);
        patch += " ";
        patch += description.members[i].struct_member_name;
        patch += ";\n";
    }
    patch += "\n};\n";
    return patch;
}

void VkExhaustiveShaderTest::CreatePassThroughVertex(std::string& source) {
    source = "#version 450\nlayout(location = 0) in vec4 position;\n";
    for (auto input_variable : input_variables) {
        if (IsBuiltinType(input_variable) == true) {
            continue;
        }
        source += "layout(location = ";
        source += std::to_string(input_variable->location);
        source += ") out ";
        GetTypeDescription(*input_variable->type_description, input_variable->format, source);
        source += " ";
        source += input_variable->name;
        source += ";\n";
    }

    source += "void main() { gl_Position = position; }\n";
}

void VkExhaustiveShaderTest::CreatePassThroughTessellationEval(std::string& source) {
    source = "#version 450\n";
    const size_t patchIndex = source.size();
    source += "layout(triangles, equal_spacing, cw) in;\n";

    for (auto output_variable : output_variables) {
        if (IsBuiltinType(output_variable) == true) {
            continue;
        }
        source += "layout(location = ";
        source += std::to_string(output_variable->location);
        source += ") in ";
        if (output_variable->type_description->type_name != nullptr) {
            std::string patch = CreateCustomTypePatch(*output_variable->type_description);
            source.insert(patchIndex, patch);
        }
        GetTypeDescription(*output_variable->type_description, output_variable->format, source);
        source += " ";
        source += output_variable->name;
        if (output_variable->type_description->op == SpvOp::SpvOpTypeArray) {
            source += "[]";
        }
        source += ";\n";
    }
    source += "void main() { gl_Position = vec4(1.0); }\n";
}

void VkExhaustiveShaderTest::CreatePassThroughTessellationControl(std::string& source) {
    source = "#version 450\n";
    const size_t patchIndex = source.size();
    source += "layout(vertices = 3) out;\n";

    for (auto input_variable : input_variables) {
        if (IsBuiltinType(input_variable) == true) {
            continue;
        }
        source += "layout(location = ";
        source += std::to_string(input_variable->location);
        source += ") out ";
        if (input_variable->type_description->type_name != nullptr) {
            std::string patch = CreateCustomTypePatch(*input_variable->type_description);
            source.insert(patchIndex, patch);
        }
        GetTypeDescription(*input_variable->type_description, input_variable->format, source);
        source += " ";
        source += input_variable->name;
        if (input_variable->type_description->op == SpvOp::SpvOpTypeArray) {
            source += "[]";
        }
        source += ";\n";
    }
    source += "void main() { }\n";
}

void VkExhaustiveShaderTest::CreateGraphicsPipeline() {
    VkPipelineObj pipeline(m_device);

    // Create image to hold data for an input attachment
    VkImageObj input_image(m_device);
    input_image.Init(m_width, m_height, 1, VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_IMAGE_TILING_OPTIMAL);
    ASSERT_TRUE(input_image.initialized());
    VkImageView input_image_view = input_image.targetView(VK_FORMAT_R8G8B8A8_UNORM);
    VkAttachmentDescription input_description = {0,
                                                 VK_FORMAT_R8G8B8A8_UNORM,
                                                 VK_SAMPLE_COUNT_1_BIT,
                                                 VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                 VK_ATTACHMENT_STORE_OP_STORE,
                                                 VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                 VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                 VK_IMAGE_LAYOUT_UNDEFINED,
                                                 VK_IMAGE_LAYOUT_GENERAL};

    uint32_t attachment_count = 0;
    // Get fragment output attachments
    if ((shader_stage & VK_SHADER_STAGE_FRAGMENT_BIT) != 0) {
        std::vector<uint32_t> fragment_attachments;
        for (auto output_variable : output_variables) {
            fragment_attachments.push_back(output_variable->location);
        }

        if (fragment_attachments.size() > 0) {
            attachment_count = *std::max_element(fragment_attachments.begin(), fragment_attachments.end());
            // locations are 0 indexed so increment by 1
            attachment_count += 1;
        }
    }

    std::vector<VkAttachmentReference> color_attachment_references(attachment_count);
    std::vector<VkPipelineColorBlendAttachmentState> color_blend_references(attachment_count);
    for (uint32_t i = 0; i < attachment_count; i++) {
        color_attachment_references[i].attachment = 0;
        color_attachment_references[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        color_blend_references[i].colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        color_blend_references[i].blendEnable = VK_FALSE;

        pipeline.AddColorAttachment(i, color_blend_references[i]);
    }

    // Find all the attachments and create a Framebuffer and RenderPass
    VkSubpassDescription subpass_dscription = {};
    subpass_dscription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_dscription.colorAttachmentCount = attachment_count;
    subpass_dscription.pColorAttachments = color_attachment_references.data();

    // Create dummy input attachments if the shader requires input attachments.
    std::vector<VkAttachmentReference> input_attachments;
    if (input_atachment_indices.size() > 0) {
        uint32_t max = *std::max_element(input_atachment_indices.begin(), input_atachment_indices.end());
        // This returns the used index, so increment by 1 to get the size
        max += 1;
        subpass_dscription.inputAttachmentCount = max;
        for (uint32_t i = 0; i < max; i++) {
            input_attachments.push_back({0, VK_IMAGE_LAYOUT_GENERAL});
        }
        subpass_dscription.pInputAttachments = input_attachments.data();
    }

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    m_renderPass_info.attachmentCount = 1;
    m_renderPass_info.pAttachments = &input_description;
    m_renderPass_info.subpassCount = 1;
    m_renderPass_info.pSubpasses = &subpass_dscription;
    m_renderPass_info.dependencyCount = 1;
    m_renderPass_info.pDependencies = &dependency;
    ASSERT_VK_SUCCESS(vk::CreateRenderPass(device(), &m_renderPass_info, nullptr, &m_renderPass));

    m_framebuffer_info.renderPass = m_renderPass;
    m_framebuffer_info.attachmentCount = 1;
    m_framebuffer_info.pAttachments = &input_image_view;
    m_framebuffer_info.width = m_width;
    m_framebuffer_info.height = m_height;
    m_framebuffer_info.layers = 1;
    ASSERT_VK_SUCCESS(vk::CreateFramebuffer(device(), &m_framebuffer_info, nullptr, &m_framebuffer));

    VkShaderObj main_module(this, "", shader_stage, SPV_ENV_VULKAN_1_0, SPV_SOURCE_BINARY, nullptr,
                            reflect_module.entry_point_name);
    main_module.InitFromBinary(spirv_data);
    pipeline.AddShader(&main_module);

    std::unique_ptr<VkShaderObj> vert_module;
    std::unique_ptr<VkShaderObj> tese_module;
    std::unique_ptr<VkShaderObj> tesc_module;

    if (shader_stage == VK_SHADER_STAGE_GEOMETRY_BIT || shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT ||
        shader_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT || shader_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
        std::string source;
        CreatePassThroughVertex(source);
        vert_module = VkShaderObj::CreateFromGLSL(*this, VK_SHADER_STAGE_VERTEX_BIT, source);
        pipeline.AddShader(vert_module.get());
    }

    if (shader_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) {
        std::string source;
        CreatePassThroughTessellationEval(source);
        tese_module = VkShaderObj::CreateFromGLSL(*this, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT, source);
        pipeline.AddShader(tese_module.get());
    }

    if (shader_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT) {
        std::string source;
        CreatePassThroughTessellationControl(source);
        tesc_module = VkShaderObj::CreateFromGLSL(*this, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT, source);
        pipeline.AddShader(tesc_module.get());
    }

    // set in case tessellation is used
    VkPipelineTessellationStateCreateInfo tessellation_state = LvlInitStruct<VkPipelineTessellationStateCreateInfo>();
    tessellation_state.patchControlPoints = 1;
    pipeline.SetTessellation(&tessellation_state);

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state = LvlInitStruct<VkPipelineInputAssemblyStateCreateInfo>();
    input_assembly_state.flags = 0;
    input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state.primitiveRestartEnable = VK_FALSE;
    if ((shader_stage == VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT) ||
        (shader_stage == VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)) {
        input_assembly_state.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    }
    pipeline.SetInputAssembly(&input_assembly_state);

    // This value is fixed and will be incorrect for a number of shaders, but it does not need to be correct in order to compile.
    VkVertexInputBindingDescription input_binding = {0, sizeof(float) * 2, VK_VERTEX_INPUT_RATE_VERTEX};
    pipeline.AddVertexInputBindings(&input_binding, 1);

    std::vector<VkVertexInputAttributeDescription> input_attributes;
    if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT) {
        for (auto input_variable : input_variables) {
            if (IsBuiltinType(input_variable) == true) {
                continue;
            }
            VkVertexInputAttributeDescription attribute = {};
            attribute.location = input_variable->location;
            attribute.binding = 0;
            attribute.format = static_cast<VkFormat>(input_variable->format);
            input_attributes.push_back(attribute);
        }
    } else {
        input_attributes.push_back({0, 0, VK_FORMAT_R32G32_SFLOAT, 0});
    }
    pipeline.AddVertexInputAttribs(input_attributes.data(), static_cast<uint32_t>(input_attributes.size()));

    VkGraphicsPipelineCreateInfo create_info = LvlInitStruct<VkGraphicsPipelineCreateInfo>();
    pipeline.InitGraphicsPipelineCreateInfo(&create_info);
    ASSERT_VK_SUCCESS(pipeline.CreateVKPipeline(pipeline_layout, m_renderPass, &create_info));
}

void VkExhaustiveShaderTest::CreateComputePipeline() {}

TEST_F(VkExhaustiveShaderTest, x) {
    m_errorMonitor->ExpectSuccess();
    ASSERT_NO_FATAL_FAILURE(InitFramework(m_errorMonitor));
    ASSERT_NO_FATAL_FAILURE(InitState());

    std::ifstream spirv_file(VkTestFramework::m_shader_path, std::ios::binary);
    size_t file_size = 0;
    if (spirv_file.good()) {
        spirv_file.seekg(0, spirv_file.end);
        file_size = spirv_file.tellg();
        spirv_data.resize(file_size / sizeof(uint32_t));
        spirv_file.seekg(0, spirv_file.beg);
        spirv_file.read(reinterpret_cast<char*>(spirv_data.data()), file_size);
        spirv_file.close();
    }

    ASSERT_REFLECT(spvReflectCreateShaderModule(file_size, reinterpret_cast<void*>(spirv_data.data()), &reflect_module));

    if (reflect_module.entry_point_count > 1) {
        GTEST_SKIP() << "Currently only single entry point modules are supported";
    }

    shader_stage = static_cast<VkShaderStageFlagBits>(reflect_module.shader_stage);

    // query using relfect to get info about the shader
    {
        uint32_t count = 0;
        ASSERT_REFLECT(spvReflectEnumerateInputVariables(&reflect_module, &count, nullptr));
        ASSERT_REFLECT(spvReflectEnumerateInputVariables(&reflect_module, &count, input_variables.data()));
        ASSERT_REFLECT(spvReflectEnumerateOutputVariables(&reflect_module, &count, nullptr));
        ASSERT_REFLECT(spvReflectEnumerateOutputVariables(&reflect_module, &count, output_variables.data()));
    }

    CreatePipelineLayout();

    const VkShaderStageFlags graphic_stages =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT |
        VK_SHADER_STAGE_GEOMETRY_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_TASK_BIT_NV | VK_SHADER_STAGE_MESH_BIT_NV;
    const VkShaderStageFlags compute_stage = VK_SHADER_STAGE_COMPUTE_BIT;
    const VkShaderStageFlags raytracing_stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                                VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                                                VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    if ((shader_stage & graphic_stages) != 0) {
        CreateGraphicsPipeline();
    } else if ((shader_stage & compute_stage) != 0) {
        CreateComputePipeline();
    } else if ((shader_stage & raytracing_stage) != 0) {
        GTEST_SKIP() << "Currently raytracing stage modules are not supported";
    }

    // clean up
    if (pipeline_layout != VK_NULL_HANDLE) {
        vk::DestroyPipelineLayout(device(), pipeline_layout, nullptr);
    }
    m_errorMonitor->VerifyNotFound();
}
