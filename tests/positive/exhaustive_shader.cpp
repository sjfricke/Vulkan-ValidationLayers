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

//
// Exhaustive Shader Test
//
// see docs/exhaustive_shader_test.md for more details

class VkExhaustiveShaderTest : public VkLayerTest {
  public:
  protected:
};

TEST_F(VkExhaustiveShaderTest, x) {
    std::ifstream spirv_file(VkTestFramework::m_shader_path, std::ios::binary);
    std::vector<uint32_t> spirv_data;
    size_t file_size = 0;
    if (spirv_file.good()) {
        spirv_file.seekg(0, spirv_file.end);
        file_size = spirv_file.tellg();
        spirv_data.resize(file_size / sizeof(uint32_t));
        spirv_file.seekg(0, spirv_file.beg);
        spirv_file.read(reinterpret_cast<char*>(spirv_data.data()), file_size);
        spirv_file.close();
    }

    SpvReflectShaderModule spv_reflect_module;
    SpvReflectResult result = spvReflectCreateShaderModule(file_size, reinterpret_cast<void*>(spirv_data.data()), &spv_reflect_module);
    ASSERT_TRUE(result == SpvReflectResult::SPV_REFLECT_RESULT_SUCCESS);
}
