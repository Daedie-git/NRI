# NRI Video TODO

## D3D12 Distinct Output/DPB Implementation

- Implement distinct D3D12 decode output and DPB setup pictures using `D3D12_VIDEO_DECODE_OUTPUT_STREAM_ARGUMENTS::ConversionArguments`.
- Query `D3D12_FEATURE_VIDEO_DECODE_CONVERSION_SUPPORT`, define how decode/output color spaces are supplied, and advertise `decodeDpbAndOutputDistinct` only when the requested conversion is supported.

## AV1 Encode Parity

- Decide whether Vulkan AV1 segmentation should remain explicitly unsupported or be implemented when driver capabilities make it viable.
- Represent and implement Vulkan CDEF, restoration, screen-content sequence options, optional picture sub-descriptors, and render sizes that differ from coded size.
- Reconcile Vulkan and D3D12 AV1 segmentation behavior so equivalent neutral descriptors produce equivalent results where supported.

## D3D11 Decode Backend

- Advertise host decode-bitstream input, the DPB texture-array requirement, and `ConfigMinRenderTargetBuffCount` from the selected decoder config as the minimum texture layer count through `VideoCapabilities`.
- Discover D3D11 video-decode support during adapter enumeration, expose `VIDEO_DECODE` as an alias of the immediate-context queue, and preserve `QueueType` through command allocator/buffer push-buffer replay.
- Implement driver profile/config discovery and the H.264 High progressive, H.265 Main/Main10, and AV1 Main decode subset; keep portable D3D11 encode unsupported.
- Snapshot host bitstream and native-argument payloads while recording, copy them into driver-owned decoder buffers during immediate-context push-buffer replay, and share neutral-to-DXVA conversion helpers with D3D12.

## Recommended Sample Coverage

- Exercise coincident and distinct decode output/DPB modes, including capability gating.
- Exercise H.264/H.265 decoded and reference-picture state transitions on Vulkan and D3D12.
- Exercise AV1 segmentation, CDEF, restoration, screen-content options, optional picture data, and render sizes distinct from coded sizes.
- Exercise encode feedback with a nonzero destination bitstream offset through explicit command-buffer resolves and a copy to host readback; use the advertised resolve queue and resolved-metadata state, and respect the advertised maximum pending-feedback count.
- Exercise decode bitstream transitions and decoded-picture readback after video-queue work on every supported backend.
- Exercise D3D11 host-bitstream decode, reference-slot reuse, DPB-array slices, and post-decode shader/copy consumption.
