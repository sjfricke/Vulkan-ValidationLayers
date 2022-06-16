# Exhaustive Shader Test

The goal of this test is to stress the SPIR-V parsing done in the validation layers. When adding new shader level validation it is easy to only test with simple test shaders, but with more and more ways to generate valid SPIR-V, it is possible to have a valid SPIR-V shader that crashes the validation layers becaues a certain code path is handling the parsing incorrectly.

The test is not checking for a validation errors, but just that the validation code can handle all valid shaders without crashing.

## How it works

After being fed a SPIR-V module the following occurs:

- Run `spirv-val` to make sure the module is indeed known to be valid.
- Use [SPIRV-Reflect](https://github.com/KhronosGroup/SPIRV-Reflect) to parse the data out of the module.
- Construct multiple valid `VkPipeline` and any pass through shaders.
    - Some shader validation path is deteremined by the pipeline information, so adjust the pipeline as needed for each variation.
    - Using the [MockICD](https://github.com/KhronosGroup/Vulkan-Tools/tree/master/icd) all the needed extensions and features can be enabled.

## Why not use Amber

Google has an amazing tool called [amber](https://github.com/google/amber) that does something similar. The main difference is for the Exhaustive Shader test, the only input is the SPIR-V module and the goal is to not crash. Amber takes in more details and goal is to make sure the correct output in produced when ran. The Exhaustive Shader Test doesn't actually plan to execute the shader, it just attempts to validate it at compile time (ex. `vkCreateGraphicsPipelines`, `vkCreateComputePipelines`, etc).