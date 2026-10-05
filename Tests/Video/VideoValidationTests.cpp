// Copyright (c) 2026 NVIDIA Corporation

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "NRI.h"

#include "Extensions/NRIDeviceCreation.h"
#include "Extensions/NRIVideo.h"

#include "SharedVal.h"

#include "BufferVal.h"
#include "CommandBufferVal.h"
#include "TextureVal.h"

nri::DeviceBase* CreateDeviceValidation(const nri::DeviceCreationDesc& desc, nri::DeviceBase& device);

namespace {

struct FakeTexture {
    nri::TextureDesc desc;
};

struct FakeBuffer {
    nri::BufferDesc desc;
};

struct FakeCommandBuffer;
class FakeVideoDevice;

struct FakeVideoSession {
    FakeVideoDevice* device;
};

class FakeVideoDevice final : public nri::DeviceBase {
public:
    FakeVideoDevice(const nri::CallbackInterface& callbacks, const nri::AllocationCallbacks& allocationCallbacks)
        : DeviceBase(callbacks, allocationCallbacks)
        , m_Session({this}) {
        m_Capabilities.maxReferenceNum = 8;
    }

    const nri::DeviceDesc& GetDesc() const override {
        return m_Desc;
    }

    void Destruct() override {
    }

    nri::Result FillFunctionTable(nri::CoreInterface& table) const override {
        table.GetBufferDesc = GetBufferDesc;
        table.GetTextureDesc = GetTextureDesc;

        return nri::Result::SUCCESS;
    }

    nri::Result FillFunctionTable(nri::HelperInterface&) const override {
        return nri::Result::SUCCESS;
    }

    nri::Result FillFunctionTable(nri::VideoInterface& table) const override {
        table.GetVideoCapabilities = GetVideoCapabilities;
        table.GetVideoAV1Capabilities = GetVideoAV1Capabilities;
        table.CreateVideoSession = CreateVideoSession;
        table.CreateVideoSessionParameters = CreateVideoSessionParameters;
        table.CreateVideoPicture = CreateVideoPicture;
        table.DestroyVideoSession = DestroyVideoSession;
        table.ResetVideoSession = ResetVideoSession;
        table.DestroyVideoSessionParameters = DestroyVideoSessionParameters;
        table.DestroyVideoPicture = DestroyVideoPicture;
        table.CmdDecodeVideo = CmdDecodeVideo;
        table.CmdEncodeVideo = CmdEncodeVideo;
        table.CmdResolveVideoEncodeFeedback = CmdResolveVideoEncodeFeedback;
        table.GetVideoEncodeFeedback = GetVideoEncodeFeedback;
        table.GetVideoEncodeAV1DecodeInfo = GetVideoEncodeAV1DecodeInfo;

        return nri::Result::SUCCESS;
    }

    void SetCapabilities(const nri::VideoCapabilities& capabilities) {
        m_Capabilities = capabilities;
    }

    uint32_t GetDecodeCallNum() const {
        return m_DecodeCallNum;
    }

    uint32_t GetEncodeCallNum() const {
        return m_EncodeCallNum;
    }

    uint32_t GetAV1DecodeInfoCallNum() const {
        return m_AV1DecodeInfoCallNum;
    }

    uint32_t GetResolveCallNum() const {
        return m_ResolveCallNum;
    }

    uint32_t GetResetCallNum() const {
        return m_ResetCallNum;
    }

private:
    friend struct FakeCommandBuffer;

    static const nri::BufferDesc& NRI_CALL GetBufferDesc(const nri::Buffer& buffer) {
        return ((const FakeBuffer&)buffer).desc;
    }

    static const nri::TextureDesc& NRI_CALL GetTextureDesc(const nri::Texture& texture) {
        return ((const FakeTexture&)texture).desc;
    }

    static nri::Result NRI_CALL GetVideoCapabilities(const nri::Device& device, const nri::VideoSessionDesc&, nri::VideoCapabilities& capabilities) {
        capabilities = ((const FakeVideoDevice&)device).m_Capabilities;

        return nri::Result::SUCCESS;
    }

    static nri::Result NRI_CALL GetVideoAV1Capabilities(const nri::Device&, const nri::VideoSessionDesc&, nri::VideoAV1Capabilities&) {
        return nri::Result::SUCCESS;
    }

    static nri::Result NRI_CALL CreateVideoSession(nri::Device& device, const nri::VideoSessionDesc&, nri::VideoSession*& session) {
        FakeVideoDevice& fakeDevice = (FakeVideoDevice&)device;
        session = (nri::VideoSession*)&fakeDevice.m_Session;

        return nri::Result::SUCCESS;
    }

    static nri::Result NRI_CALL CreateVideoSessionParameters(nri::Device& device, const nri::VideoSessionParametersDesc&, nri::VideoSessionParameters*& parameters) {
        FakeVideoDevice& fakeDevice = (FakeVideoDevice&)device;
        parameters = (nri::VideoSessionParameters*)&fakeDevice.m_Parameters;

        return nri::Result::SUCCESS;
    }

    static nri::Result NRI_CALL CreateVideoPicture(nri::Device& device, const nri::VideoPictureDesc&, nri::VideoPicture*& picture) {
        FakeVideoDevice& fakeDevice = (FakeVideoDevice&)device;
        picture = (nri::VideoPicture*)&fakeDevice.m_Picture;

        return nri::Result::SUCCESS;
    }

    static void NRI_CALL DestroyVideoSession(nri::VideoSession*) {
    }

    static void NRI_CALL ResetVideoSession(nri::VideoSession& session) {
        ((FakeVideoSession&)session).device->m_ResetCallNum++;
    }

    static void NRI_CALL DestroyVideoSessionParameters(nri::VideoSessionParameters*) {
    }

    static void NRI_CALL DestroyVideoPicture(nri::VideoPicture*) {
    }

    static void NRI_CALL CmdDecodeVideo(nri::CommandBuffer& commandBuffer, const nri::VideoDecodeDesc&);
    static void NRI_CALL CmdEncodeVideo(nri::CommandBuffer& commandBuffer, const nri::VideoEncodeDesc&);

    static void NRI_CALL CmdResolveVideoEncodeFeedback(nri::CommandBuffer& commandBuffer, nri::VideoSession&, nri::Buffer&, uint64_t);

    static nri::Result NRI_CALL GetVideoEncodeFeedback(nri::VideoSession&, nri::Buffer&, uint64_t, nri::VideoEncodeFeedback&) {
        return nri::Result::SUCCESS;
    }

    static nri::Result NRI_CALL GetVideoEncodeAV1DecodeInfo(nri::VideoSession& session, nri::Buffer&, uint64_t, const nri::VideoAV1EncodeDecodeInfoDesc&, nri::VideoAV1EncodeDecodeInfo&) {
        ((FakeVideoSession&)session).device->m_AV1DecodeInfoCallNum++;

        return nri::Result::SUCCESS;
    }

    nri::DeviceDesc m_Desc = {};
    nri::VideoCapabilities m_Capabilities = {};
    FakeVideoSession m_Session;
    uint8_t m_Parameters = 0;
    uint8_t m_Picture = 0;
    uint32_t m_DecodeCallNum = 0;
    uint32_t m_EncodeCallNum = 0;
    uint32_t m_AV1DecodeInfoCallNum = 0;
    uint32_t m_ResolveCallNum = 0;
    uint32_t m_ResetCallNum = 0;
};

struct FakeCommandBuffer {
    FakeVideoDevice* device;
};

void NRI_CALL FakeVideoDevice::CmdDecodeVideo(nri::CommandBuffer& commandBuffer, const nri::VideoDecodeDesc&) {
    ((FakeCommandBuffer&)commandBuffer).device->m_DecodeCallNum++;
}

void NRI_CALL FakeVideoDevice::CmdEncodeVideo(nri::CommandBuffer& commandBuffer, const nri::VideoEncodeDesc&) {
    ((FakeCommandBuffer&)commandBuffer).device->m_EncodeCallNum++;
}

void NRI_CALL FakeVideoDevice::CmdResolveVideoEncodeFeedback(nri::CommandBuffer& commandBuffer, nri::VideoSession&, nri::Buffer&, uint64_t) {
    ((FakeCommandBuffer&)commandBuffer).device->m_ResolveCallNum++;
}

struct AllocationHeader {
    void* memory;
    size_t size;
};

static void* NRI_CALL Allocate(void*, size_t size, size_t alignment) {
    const size_t effectiveAlignment = std::max(alignment, alignof(AllocationHeader));

    if ((effectiveAlignment & (effectiveAlignment - 1)) || effectiveAlignment > std::numeric_limits<size_t>::max() - sizeof(AllocationHeader) + 1)
        return nullptr;

    const size_t overhead = sizeof(AllocationHeader) + effectiveAlignment - 1;

    if (size > std::numeric_limits<size_t>::max() - overhead)
        return nullptr;

    uint8_t* memory = (uint8_t*)std::malloc(size + overhead);

    if (!memory)
        return nullptr;

    uint8_t* alignedMemory = nri::Align(memory + sizeof(AllocationHeader), effectiveAlignment);
    AllocationHeader* header = (AllocationHeader*)alignedMemory - 1;
    header->memory = memory;
    header->size = size;

    return alignedMemory;
}

static void* NRI_CALL Reallocate(void* userArg, void* memory, size_t size, size_t alignment) {
    if (!memory)
        return Allocate(userArg, size, alignment);

    AllocationHeader* oldHeader = (AllocationHeader*)memory - 1;
    void* newMemory = Allocate(userArg, size, alignment);

    if (!newMemory)
        return nullptr;

    std::memcpy(newMemory, memory, std::min(size, oldHeader->size));
    std::free(oldHeader->memory);

    return newMemory;
}

static void NRI_CALL Free(void*, void* memory) {
    if (memory)
        std::free(((AllocationHeader*)memory - 1)->memory);
}

static void NRI_CALL IgnoreMessage(nri::Message, const char*, uint32_t, const char*, void*) {
}

class ValidationHarness {
public:
    ValidationHarness()
        : m_AllocationCallbacks({Allocate, Reallocate, Free})
        , m_Callbacks({IgnoreMessage})
        , m_Backend(m_Callbacks, m_AllocationCallbacks) {
        nri::DeviceCreationDesc desc = {};
        desc.allocationCallbacks = m_AllocationCallbacks;
        desc.callbackInterface = m_Callbacks;
        m_Device = CreateDeviceValidation(desc, m_Backend);

        if (m_Device)
            m_IsValid = m_Device->FillFunctionTable(m_Video) == nri::Result::SUCCESS;
    }

    ~ValidationHarness() {
        for (nri::VideoPicture* picture : m_Pictures)
            m_Video.DestroyVideoPicture(picture);

        for (nri::VideoSessionParameters* parameters : m_Parameters)
            m_Video.DestroyVideoSessionParameters(parameters);

        for (nri::VideoSession* session : m_Sessions)
            m_Video.DestroyVideoSession(session);

        if (m_Device)
            m_Device->Destruct();
    }

    bool IsValid() const {
        return m_IsValid;
    }

    nri::Device& GetDevice() const {
        return *(nri::Device*)m_Device;
    }

    nri::DeviceVal& GetDeviceVal() const {
        return *(nri::DeviceVal*)m_Device;
    }

    FakeVideoDevice& GetBackend() {
        return m_Backend;
    }

    nri::VideoInterface& GetVideo() {
        return m_Video;
    }

    nri::VideoSession* CreateSession(const nri::VideoSessionDesc& desc) {
        nri::VideoSession* session = nullptr;

        if (m_Video.CreateVideoSession(GetDevice(), desc, session) == nri::Result::SUCCESS)
            m_Sessions.push_back(session);

        return session;
    }

    nri::VideoSessionParameters* CreateParameters(nri::VideoSession& session, const nri::VideoSessionParametersDesc& source) {
        nri::VideoSessionParametersDesc desc = source;
        desc.session = &session;

        nri::VideoSessionParameters* parameters = nullptr;

        if (m_Video.CreateVideoSessionParameters(GetDevice(), desc, parameters) == nri::Result::SUCCESS)
            m_Parameters.push_back(parameters);

        return parameters;
    }

    nri::VideoPicture* CreatePicture(nri::VideoPictureUsage usage, nri::Format format, uint32_t width, uint32_t height, nri::TextureUsageBits textureUsage, nri::TextureType textureType = nri::TextureType::TEXTURE_2D, uint16_t layerNum = 1, uint16_t layer = 0, nri::VideoCodec videoCodec = nri::VideoCodec::H264) {
        FakeTexture texture = {};
        texture.desc.type = textureType;
        texture.desc.usage = textureUsage;
        texture.desc.format = format;
        texture.desc.videoCodec = videoCodec;
        texture.desc.width = (nri::Dim_t)width;
        texture.desc.height = (nri::Dim_t)height;
        texture.desc.layerNum = layerNum;
        nri::TextureVal textureVal(GetDeviceVal(), (nri::Texture*)&texture, true);
        nri::VideoPictureDesc desc = {};
        desc.texture = (nri::Texture*)&textureVal;
        desc.usage = usage;
        desc.width = (nri::Dim_t)width;
        desc.height = (nri::Dim_t)height;
        desc.layer = layer;

        nri::VideoPicture* picture = nullptr;

        if (m_Video.CreateVideoPicture(GetDevice(), desc, picture) == nri::Result::SUCCESS)
            m_Pictures.push_back(picture);

        return picture;
    }

private:
    nri::AllocationCallbacks m_AllocationCallbacks;
    nri::CallbackInterface m_Callbacks;
    FakeVideoDevice m_Backend;
    nri::DeviceBase* m_Device = nullptr;
    nri::VideoInterface m_Video = {};
    std::vector<nri::VideoSession*> m_Sessions;
    std::vector<nri::VideoSessionParameters*> m_Parameters;
    std::vector<nri::VideoPicture*> m_Pictures;
    bool m_IsValid = false;
};

static nri::VideoSessionDesc MakeSessionDesc(nri::VideoSessionType type = nri::VideoSessionType::DECODE, nri::VideoCodec codec = nri::VideoCodec::H264) {
    nri::VideoSessionDesc desc = {};
    desc.type = type;
    desc.codec = codec;
    desc.format = nri::Format::NV12_UNORM;
    desc.width = 1920;
    desc.height = 1080;
    desc.maxReferenceNum = 4;

    return desc;
}

static FakeBuffer MakeBuffer(uint64_t size, nri::BufferUsageBits usage = nri::BufferUsageBits::NONE) {
    FakeBuffer buffer = {};
    buffer.desc.size = size;
    buffer.desc.usage = usage;

    return buffer;
}

static nri::VideoSessionParameters* CreateH264Parameters(ValidationHarness& harness, nri::VideoSession& session) {
    static const nri::VideoH264SequenceParameterSetDesc sequence = {};
    nri::VideoH264SessionParametersDesc h264 = {};
    h264.sequenceParameterSets = &sequence;
    h264.sequenceParameterSetNum = 1;
    nri::VideoSessionParametersDesc desc = {};
    desc.h264Parameters = &h264;

    return harness.CreateParameters(session, desc);
}

} // namespace

TEST_CASE("VID-VAL-001 video session descriptors reject every invalid discriminator", "[video][validation][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSessionDesc desc = MakeSessionDesc();
    nri::VideoCapabilities capabilities = {};
    REQUIRE(harness.GetVideo().GetVideoCapabilities(harness.GetDevice(), desc, capabilities) == nri::Result::SUCCESS);

    SECTION("session type") {
        desc.type = nri::VideoSessionType::MAX_NUM;
    }
    SECTION("codec none") {
        desc.codec = nri::VideoCodec::NONE;
    }
    SECTION("codec maximum") {
        desc.codec = nri::VideoCodec::MAX_NUM;
    }
    SECTION("format") {
        desc.format = nri::Format::UNKNOWN;
    }
    SECTION("dimensions") {
        desc.width = 0;
    }
    SECTION("slot overflow") {
        desc.maxReferenceNum = UINT32_MAX;
    }

    REQUIRE(harness.GetVideo().GetVideoCapabilities(harness.GetDevice(), desc, capabilities) == nri::Result::INVALID_ARGUMENT);
}

TEST_CASE("VID-VAL-002 session parameters enforce codec and pointer-count contracts", "[video][validation][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* h264Session = harness.CreateSession(MakeSessionDesc());
    REQUIRE(h264Session);

    nri::VideoH264SequenceParameterSetDesc h264Sps = {};
    nri::VideoH264SessionParametersDesc h264 = {};
    h264.sequenceParameterSets = &h264Sps;
    h264.sequenceParameterSetNum = 1;
    nri::VideoSessionParametersDesc desc = {};
    desc.h264Parameters = &h264;
    REQUIRE(harness.CreateParameters(*h264Session, desc));

    SECTION("multiple codec payloads") {
        nri::VideoAV1SessionParametersDesc av1 = {};
        desc.av1Parameters = &av1;
        REQUIRE_FALSE(harness.CreateParameters(*h264Session, desc));
    }
    SECTION("missing counted array") {
        h264.sequenceParameterSets = nullptr;
        REQUIRE_FALSE(harness.CreateParameters(*h264Session, desc));
    }
    SECTION("capacity smaller than initial data") {
        h264.maxSequenceParameterSetNum = 1;
        h264.sequenceParameterSetNum = 2;
        REQUIRE_FALSE(harness.CreateParameters(*h264Session, desc));
    }
    SECTION("wrong codec") {
        nri::VideoSession* h265Session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::DECODE, nri::VideoCodec::H265));
        REQUIRE(h265Session);
        REQUIRE_FALSE(harness.CreateParameters(*h265Session, desc));
    }
    SECTION("nested H.265 arrays") {
        nri::VideoSession* h265Session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::DECODE, nri::VideoCodec::H265));
        REQUIRE(h265Session);
        nri::VideoH265SequenceParameterSetDesc h265Sps = {};
        h265Sps.numShortTermRefPicSets = 1;
        nri::VideoH265SessionParametersDesc h265 = {};
        h265.sequenceParameterSets = &h265Sps;
        h265.sequenceParameterSetNum = 1;
        nri::VideoSessionParametersDesc h265Desc = {};
        h265Desc.h265Parameters = &h265;
        REQUIRE_FALSE(harness.CreateParameters(*h265Session, h265Desc));

        nri::VideoH265ShortTermRefPicSetDesc shortTerm = {};
        h265Sps.shortTermRefPicSets = &shortTerm;
        REQUIRE(harness.CreateParameters(*h265Session, h265Desc));
    }
}

TEST_CASE("VID-VAL-003 video pictures validate texture shape, usage, and subresource", "[video][validation][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    REQUIRE(harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE, nri::TextureType::TEXTURE_2D, 2, 1));
    REQUIRE_FALSE(harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE, nri::TextureType::TEXTURE_2D, 2, 2));
    REQUIRE_FALSE(harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE));
    REQUIRE_FALSE(harness.CreatePicture(nri::VideoPictureUsage::DECODE_REFERENCE, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE, nri::TextureType::TEXTURE_3D));
}

TEST_CASE("VID-VAL-004 AV1 aliases preserve DPB identity", "[video][validation][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::ENCODE, nri::VideoCodec::AV1));
    REQUIRE(session);

    FakeBuffer buffer = MakeBuffer(64);
    nri::BufferVal bufferVal(harness.GetDeviceVal(), (nri::Buffer*)&buffer, true);
    nri::VideoEncodeFeedback feedback = {};
    nri::VideoAV1SequenceDesc sequence = {};
    std::array<uint8_t, 8> savedOrderHints = {};
    std::array<nri::VideoAV1ReferenceDesc, 2> references = {};
    references[0].name = nri::VideoAV1ReferenceName::LAST;
    references[0].refFrameIndex = 3;
    references[0].frameType = nri::VideoEncodeFrameType::P;
    references[0].orderHint = 4;
    references[0].frameId = 10;
    references[0].slot = 3;
    references[0].savedOrderHints = savedOrderHints.data();
    references[1] = references[0];
    references[1].name = nri::VideoAV1ReferenceName::GOLDEN;
    nri::VideoAV1EncodeDecodeInfoDesc desc = {};
    desc.feedback = &feedback;
    desc.sequence = &sequence;
    desc.references = references.data();
    desc.referenceNum = (uint32_t)references.size();
    nri::VideoAV1EncodeDecodeInfo info = {};

    REQUIRE(harness.GetVideo().GetVideoEncodeAV1DecodeInfo(*session, (nri::Buffer&)bufferVal, 0, desc, info) == nri::Result::SUCCESS);
    REQUIRE(harness.GetBackend().GetAV1DecodeInfoCallNum() == 1);

    references[1].frameId++;
    REQUIRE(harness.GetVideo().GetVideoEncodeAV1DecodeInfo(*session, (nri::Buffer&)bufferVal, 0, desc, info) == nri::Result::INVALID_ARGUMENT);

    references[1] = references[0];
    references[1].name = nri::VideoAV1ReferenceName::GOLDEN;
    references[1].refFrameIndex++;
    references[1].frameId++;
    REQUIRE(harness.GetVideo().GetVideoEncodeAV1DecodeInfo(*session, (nri::Buffer&)bufferVal, 0, desc, info) == nri::Result::INVALID_ARGUMENT);

    references[1] = references[0];
    REQUIRE(harness.GetVideo().GetVideoEncodeAV1DecodeInfo(*session, (nri::Buffer&)bufferVal, 0, desc, info) == nri::Result::INVALID_ARGUMENT);
    REQUIRE(harness.GetBackend().GetAV1DecodeInfoCallNum() == 1);
}

TEST_CASE("VID-REG-006 decode DPB slots stay within the session", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* session = harness.CreateSession(MakeSessionDesc());
    REQUIRE(session);
    nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
    REQUIRE(parameters);
    nri::VideoPicture* dstPicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
    nri::VideoPicture* referencePicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_REFERENCE, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
    REQUIRE(dstPicture);
    REQUIRE(referencePicture);

    FakeBuffer bitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_DECODE);
    nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
    nri::VideoReference reference = {};
    reference.picture = referencePicture;
    reference.slot = 4;
    nri::VideoDecodeDesc desc = {};
    desc.session = session;
    desc.parameters = parameters;
    desc.bitstream.buffer = (nri::Buffer*)&bitstreamVal;
    desc.bitstream.size = 64;
    desc.dstPicture = dstPicture;
    desc.references = &reference;
    desc.referenceNum = 1;
    desc.dstSlot = 4;

    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);

    nri::VideoH264DecodePictureDesc setupPictureDesc = {};
    setupPictureDesc.hasReferenceSlot = true;
    setupPictureDesc.referenceSlot = 4;
    desc.h264PictureDesc = &setupPictureDesc;
    desc.dstSlot = UINT32_MAX;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 2);

    desc.h264PictureDesc = nullptr;
    desc.dstSlot = 4;
    reference.slot = 5;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 2);
    reference.slot = 4;
    desc.dstSlot = UINT32_MAX;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 2);

    nri::VideoH264DecodeReferenceDesc codecReference = {};
    codecReference.slot = 5;
    nri::VideoH264DecodePictureDesc picture = {};
    picture.hasReferenceSlot = true;
    picture.referenceSlot = 5;
    picture.references = &codecReference;
    picture.referenceNum = 1;
    desc.dstSlot = 4;
    desc.h264PictureDesc = &picture;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 2);
}

TEST_CASE("VID-REG-007 encode DPB slots stay within the session", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::ENCODE));
    REQUIRE(session);
    nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
    REQUIRE(parameters);
    nri::VideoPicture* srcPicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_ENCODE);
    nri::VideoPicture* reconstructedPicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_REFERENCE, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_ENCODE);
    REQUIRE(srcPicture);
    REQUIRE(reconstructedPicture);

    FakeBuffer bitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
    nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
    nri::VideoReference reference = {};
    reference.picture = reconstructedPicture;
    reference.slot = 4;
    nri::VideoEncodePictureDesc encodePicture = {};
    encodePicture.frameType = nri::VideoEncodeFrameType::P;
    nri::VideoH264ReferenceDesc codecReference = {};
    codecReference.frameType = nri::VideoEncodeFrameType::P;
    codecReference.listIndex = 0;
    codecReference.slot = 4;
    nri::VideoH264PictureDesc picture = {};
    picture.references = &codecReference;
    picture.referenceNum = 1;
    nri::VideoEncodeDesc desc = {};
    desc.session = session;
    desc.parameters = parameters;
    desc.srcPicture = srcPicture;
    desc.dstBitstream.buffer = (nri::Buffer*)&bitstreamVal;
    desc.dstBitstream.size = 64;
    desc.pictureDesc = &encodePicture;
    desc.reconstructedPicture = reconstructedPicture;
    desc.references = &reference;
    desc.referenceNum = 1;
    desc.reconstructedSlot = 4;
    desc.h264PictureDesc = &picture;

    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

    reference.slot = 5;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
    reference.slot = 4;
    desc.reconstructedSlot = UINT32_MAX;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

    desc.reconstructedSlot = 4;
    codecReference.slot = 5;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
}

TEST_CASE("VID-REG-011 session formats stay within backend conversion tables", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSessionDesc desc = MakeSessionDesc();
    desc.format = nri::Format::MAX_NUM;
    nri::VideoCapabilities capabilities = {};
    REQUIRE(harness.GetVideo().GetVideoCapabilities(harness.GetDevice(), desc, capabilities) == nri::Result::INVALID_ARGUMENT);
}

TEST_CASE("VID-VAL-005 session recording-state reset is forwarded to the backend", "[video][validation][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* session = harness.CreateSession(MakeSessionDesc());
    REQUIRE(session);
    REQUIRE(harness.GetBackend().GetResetCallNum() == 0);

    harness.GetVideo().ResetVideoSession(*session);
    REQUIRE(harness.GetBackend().GetResetCallNum() == 1);
}

TEST_CASE("VID-REG-012 video metadata ranges use checked subtraction", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoCapabilities capabilities = {};
    capabilities.maxReferenceNum = 4;
    capabilities.metadataOffsetAlignment = 4;
    capabilities.resolvedMetadataOffsetAlignment = 4;
    capabilities.metadataSize = 16;
    capabilities.resolvedMetadataSize = 16;
    harness.GetBackend().SetCapabilities(capabilities);
    nri::VideoSession* session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::ENCODE));
    REQUIRE(session);
    nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
    REQUIRE(parameters);
    nri::VideoPicture* srcPicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_ENCODE);
    REQUIRE(srcPicture);

    FakeBuffer bitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
    FakeBuffer metadata = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
    FakeBuffer resolvedMetadata = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
    nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
    nri::BufferVal metadataVal(harness.GetDeviceVal(), (nri::Buffer*)&metadata, true);
    nri::BufferVal resolvedMetadataVal(harness.GetDeviceVal(), (nri::Buffer*)&resolvedMetadata, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
    nri::VideoEncodeDesc desc = {};
    desc.session = session;
    desc.parameters = parameters;
    desc.srcPicture = srcPicture;
    desc.dstBitstream.buffer = (nri::Buffer*)&bitstreamVal;
    desc.dstBitstream.size = 64;
    desc.metadata = (nri::Buffer*)&metadataVal;
    desc.metadataOffset = 48;
    desc.resolvedMetadata = (nri::Buffer*)&resolvedMetadataVal;
    desc.resolvedMetadataOffset = 48;

    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

    desc.metadataOffset = 49;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
    desc.metadataOffset = 48;
    desc.resolvedMetadataOffset = 52;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

    capabilities.metadataSize = 4;
    capabilities.resolvedMetadataSize = 0;
    harness.GetBackend().SetCapabilities(capabilities);
    FakeBuffer hugeMetadata = MakeBuffer(UINT64_MAX, nri::BufferUsageBits::VIDEO_ENCODE);
    nri::BufferVal hugeMetadataVal(harness.GetDeviceVal(), (nri::Buffer*)&hugeMetadata, true);
    desc.metadata = (nri::Buffer*)&hugeMetadataVal;
    desc.metadataOffset = UINT64_MAX - 1;
    desc.resolvedMetadata = nullptr;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
}

TEST_CASE("VID-REG-013 video pictures match their session format and coded extent", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSession* decodeSession = harness.CreateSession(MakeSessionDesc());
    REQUIRE(decodeSession);
    nri::VideoSessionParameters* decodeParameters = CreateH264Parameters(harness, *decodeSession);
    REQUIRE(decodeParameters);
    nri::VideoPicture* validDecodePicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1280, 720, nri::TextureUsageBits::VIDEO_DECODE);
    nri::VideoPicture* wrongFormatPicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::P010_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
    REQUIRE(validDecodePicture);
    REQUIRE(wrongFormatPicture);

    FakeBuffer decodeBitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_DECODE);
    nri::BufferVal decodeBitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&decodeBitstream, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
    nri::VideoDecodeDesc decodeDesc = {};
    decodeDesc.session = decodeSession;
    decodeDesc.parameters = decodeParameters;
    decodeDesc.bitstream.buffer = (nri::Buffer*)&decodeBitstreamVal;
    decodeDesc.bitstream.size = 64;
    decodeDesc.dstPicture = validDecodePicture;
    decodeDesc.dstSlot = 0;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, decodeDesc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);
    decodeDesc.dstPicture = wrongFormatPicture;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, decodeDesc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);

    nri::VideoSession* encodeSession = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::ENCODE));
    REQUIRE(encodeSession);
    nri::VideoSessionParameters* encodeParameters = CreateH264Parameters(harness, *encodeSession);
    REQUIRE(encodeParameters);
    nri::VideoPicture* smallEncodePicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1280, 720, nri::TextureUsageBits::VIDEO_ENCODE);
    REQUIRE(smallEncodePicture);

    FakeBuffer encodeBitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
    nri::BufferVal encodeBitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&encodeBitstream, true);
    nri::VideoEncodeDesc encodeDesc = {};
    encodeDesc.session = encodeSession;
    encodeDesc.parameters = encodeParameters;
    encodeDesc.srcPicture = smallEncodePicture;
    encodeDesc.dstBitstream.buffer = (nri::Buffer*)&encodeBitstreamVal;
    encodeDesc.dstBitstream.size = 64;
    harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, encodeDesc);
    REQUIRE(harness.GetBackend().GetEncodeCallNum() == 0);

    nri::VideoSession* h265Session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::DECODE, nri::VideoCodec::H265));
    REQUIRE(h265Session);
    nri::VideoSessionParametersDesc h265ParametersDesc = {};
    nri::VideoSessionParameters* h265Parameters = harness.CreateParameters(*h265Session, h265ParametersDesc);
    REQUIRE(h265Parameters);
    nri::VideoPicture* h264Picture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
    REQUIRE(h264Picture);

    nri::VideoDecodeDesc wrongCodecDesc = {};
    wrongCodecDesc.session = h265Session;
    wrongCodecDesc.parameters = h265Parameters;
    wrongCodecDesc.bitstream.buffer = (nri::Buffer*)&decodeBitstreamVal;
    wrongCodecDesc.bitstream.size = 64;
    wrongCodecDesc.dstPicture = h264Picture;
    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, wrongCodecDesc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);
}

TEST_CASE("VID-REG-026 video bitstream ranges satisfy session alignment", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoCapabilities capabilities = {};
    capabilities.maxReferenceNum = 4;
    capabilities.bitstreamOffsetAlignment = 4;
    capabilities.bitstreamSizeAlignment = 8;
    harness.GetBackend().SetCapabilities(capabilities);

    SECTION("decode") {
        nri::VideoSession* session = harness.CreateSession(MakeSessionDesc());
        REQUIRE(session);
        nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
        REQUIRE(parameters);
        nri::VideoPicture* dstPicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
        REQUIRE(dstPicture);

        FakeBuffer bitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_DECODE);
        nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
        FakeCommandBuffer command = {&harness.GetBackend()};
        nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
        nri::VideoDecodeDesc desc = {};
        desc.session = session;
        desc.parameters = parameters;
        desc.bitstream.buffer = (nri::Buffer*)&bitstreamVal;
        desc.bitstream.size = 64;
        desc.dstPicture = dstPicture;

        harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);

        desc.bitstream.offset = 2;
        desc.bitstream.size = 56;
        harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);

        desc.bitstream.offset = 0;
        desc.bitstream.size = 60;
        harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetDecodeCallNum() == 1);
    }

    SECTION("encode") {
        nri::VideoSession* session = harness.CreateSession(MakeSessionDesc(nri::VideoSessionType::ENCODE));
        REQUIRE(session);
        nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
        REQUIRE(parameters);
        nri::VideoPicture* srcPicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_ENCODE);
        REQUIRE(srcPicture);

        FakeBuffer bitstream = MakeBuffer(64, nri::BufferUsageBits::VIDEO_ENCODE);
        nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
        FakeCommandBuffer command = {&harness.GetBackend()};
        nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);
        nri::VideoEncodeDesc desc = {};
        desc.session = session;
        desc.parameters = parameters;
        desc.srcPicture = srcPicture;
        desc.dstBitstream.buffer = (nri::Buffer*)&bitstreamVal;
        desc.dstBitstream.size = 64;

        harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

        desc.dstBitstream.offset = 2;
        desc.dstBitstream.size = 56;
        harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);

        desc.dstBitstream.offset = 0;
        desc.dstBitstream.size = 60;
        harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
    }
}

TEST_CASE("VID-REG-021 default video session parameters are optional", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSessionDesc sessionDesc = MakeSessionDesc();
    nri::VideoSession* session = harness.CreateSession(sessionDesc);
    REQUIRE(session);

    nri::VideoSessionParametersDesc parametersDesc = {};
    REQUIRE(harness.CreateParameters(*session, parametersDesc));
}

TEST_CASE("VID-REG-022 resolved metadata does not require bitstream usage", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoCapabilities capabilities = {};
    capabilities.maxReferenceNum = 4;
    capabilities.resolvedMetadataOffsetAlignment = 4;
    capabilities.resolvedMetadataSize = 16;
    harness.GetBackend().SetCapabilities(capabilities);

    nri::VideoSessionDesc sessionDesc = MakeSessionDesc(nri::VideoSessionType::ENCODE);
    nri::VideoSession* session = harness.CreateSession(sessionDesc);
    REQUIRE(session);

    FakeBuffer resolvedMetadata = {};
    resolvedMetadata.desc.size = 64;
    resolvedMetadata.desc.usage = nri::BufferUsageBits::NONE;
    nri::BufferVal resolvedMetadataVal(harness.GetDeviceVal(), (nri::Buffer*)&resolvedMetadata, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);

    SECTION("encode") {
        nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
        REQUIRE(parameters);
        nri::VideoPicture* srcPicture = harness.CreatePicture(nri::VideoPictureUsage::ENCODE_INPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_ENCODE);
        REQUIRE(srcPicture);

        FakeBuffer bitstream = {};
        bitstream.desc.size = 64;
        bitstream.desc.usage = nri::BufferUsageBits::VIDEO_ENCODE;
        nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
        nri::VideoEncodeDesc desc = {};
        desc.session = session;
        desc.parameters = parameters;
        desc.srcPicture = srcPicture;
        desc.dstBitstream.buffer = (nri::Buffer*)&bitstreamVal;
        desc.dstBitstream.size = 64;
        desc.resolvedMetadata = (nri::Buffer*)&resolvedMetadataVal;

        harness.GetVideo().CmdEncodeVideo((nri::CommandBuffer&)commandVal, desc);
        REQUIRE(harness.GetBackend().GetEncodeCallNum() == 1);
    }

    SECTION("resolve") {
        harness.GetVideo().CmdResolveVideoEncodeFeedback((nri::CommandBuffer&)commandVal, *session, (nri::Buffer&)resolvedMetadataVal, 0);
        REQUIRE(harness.GetBackend().GetResolveCallNum() == 1);
    }
}

TEST_CASE("VID-REG-023 codec references stay within the session capacity", "[video][validation][regression][short]") {
    ValidationHarness harness;
    REQUIRE(harness.IsValid());

    nri::VideoSessionDesc sessionDesc = MakeSessionDesc();
    sessionDesc.maxReferenceNum = 4;
    nri::VideoSession* session = harness.CreateSession(sessionDesc);
    REQUIRE(session);
    nri::VideoSessionParameters* parameters = CreateH264Parameters(harness, *session);
    REQUIRE(parameters);
    nri::VideoPicture* dstPicture = harness.CreatePicture(nri::VideoPictureUsage::DECODE_OUTPUT, nri::Format::NV12_UNORM, 1920, 1080, nri::TextureUsageBits::VIDEO_DECODE);
    REQUIRE(dstPicture);

    FakeBuffer bitstream = {};
    bitstream.desc.size = 64;
    bitstream.desc.usage = nri::BufferUsageBits::VIDEO_DECODE;
    nri::BufferVal bitstreamVal(harness.GetDeviceVal(), (nri::Buffer*)&bitstream, true);
    FakeCommandBuffer command = {&harness.GetBackend()};
    nri::CommandBufferVal commandVal(harness.GetDeviceVal(), (nri::CommandBuffer*)&command, true);

    std::array<nri::VideoH264DecodeReferenceDesc, 5> codecReferences = {};

    for (uint32_t i = 0; i < codecReferences.size(); i++)
        codecReferences[i].slot = i;

    nri::VideoH264DecodePictureDesc picture = {};
    picture.references = codecReferences.data();
    picture.referenceNum = (uint32_t)codecReferences.size();
    nri::VideoDecodeDesc desc = {};
    desc.session = session;
    desc.parameters = parameters;
    desc.bitstream.buffer = (nri::Buffer*)&bitstreamVal;
    desc.bitstream.size = 64;
    desc.dstPicture = dstPicture;
    desc.h264PictureDesc = &picture;

    harness.GetVideo().CmdDecodeVideo((nri::CommandBuffer&)commandVal, desc);
    REQUIRE(harness.GetBackend().GetDecodeCallNum() == 0);
}
