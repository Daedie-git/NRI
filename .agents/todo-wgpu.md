# NRI WGPU TODO

- Define a meaningful `DeviceDesc::shaderModel` value for WebGPU/WGSL; the current `NriShaderModel(6, 0)` is a compatibility placeholder.
- Complete occlusion-query support: attach the query set to render passes, then advertise `features.occlusion`.
- Replace or extend the limited WGSL direct-declaration texture parser with reliable reflection.
- Complete descriptor-layout metadata:
  - distinguish descriptor-set comparison samplers from filtering samplers;
  - represent sampled-texture format and filterability so 32-bit float textures can use `UnfilterableFloat` layouts where required.
- Add a WGSL sample or focused test.
- Gate fixed-size descriptor arrays on native WGPU binding-array feature support through `DeviceDesc` and Validation.
- Define support and capability semantics for `DescriptorRangeBits::ALLOW_UPDATE_AFTER_SET`; bind-group recreation handles update and rebind, not updates after a recorded binding.
- Resolve the indirect-count/stride API mismatch before advertising `features.drawIndirectCount`.
- Decide whether and how to support `NRI_DRAW_ID` / `PipelineLayoutBits::ENABLE_DRAW_INDEX_EMULATION`; keep `shaderFeatures.drawIndex` false until then.
- Install device-lost and uncaptured-error callbacks and forward messages through NRI callbacks.
