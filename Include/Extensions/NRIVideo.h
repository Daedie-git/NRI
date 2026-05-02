// © 2021 NVIDIA Corporation

// Goal: backend-neutral hardware video encode/decode command submission
// Video formats: https://learn.microsoft.com/en-us/windows/win32/medfound/recommended-8-bit-yuv-formats-for-video-rendering

#pragma once

#define NRI_VIDEO_H 1

NriNamespaceBegin

NriForwardStruct(VideoSession);
NriForwardStruct(VideoSessionParameters);
NriForwardStruct(VideoPicture);

NriEnum(VideoUsage, uint8_t,
    DECODE,
    ENCODE
);

NriEnum(VideoCodec, uint8_t,
    H264,
    H265,
    AV1
);

NriEnum(VideoDecodeArgumentType, uint8_t,
    PICTURE_PARAMETERS,
    INVERSE_QUANTIZATION_MATRIX,
    SLICE_CONTROL
);

NriBits(VideoH264SequenceParameterSetBits, uint16_t,
    NONE                                = 0,
    CONSTRAINT_SET0                     = NriBit(0),
    CONSTRAINT_SET1                     = NriBit(1),
    CONSTRAINT_SET2                     = NriBit(2),
    CONSTRAINT_SET3                     = NriBit(3),
    CONSTRAINT_SET4                     = NriBit(4),
    CONSTRAINT_SET5                     = NriBit(5),
    DIRECT_8X8_INFERENCE                = NriBit(6),
    MB_ADAPTIVE_FRAME_FIELD             = NriBit(7),
    FRAME_MBS_ONLY                      = NriBit(8),
    DELTA_PIC_ORDER_ALWAYS_ZERO         = NriBit(9),
    SEPARATE_COLOUR_PLANE               = NriBit(10),
    GAPS_IN_FRAME_NUM_ALLOWED           = NriBit(11),
    QPPRIME_Y_ZERO_TRANSFORM_BYPASS     = NriBit(12)
);

NriBits(VideoH264PictureParameterSetBits, uint8_t,
    NONE                                = 0,
    TRANSFORM_8X8_MODE                  = NriBit(0),
    REDUNDANT_PIC_CNT_PRESENT           = NriBit(1),
    CONSTRAINED_INTRA_PRED              = NriBit(2),
    DEBLOCKING_FILTER_CONTROL_PRESENT   = NriBit(3),
    WEIGHTED_PRED                       = NriBit(4),
    BOTTOM_FIELD_PIC_ORDER_IN_FRAME     = NriBit(5),
    ENTROPY_CODING_MODE                 = NriBit(6)
);

NriBits(VideoH264DecodePictureBits, uint8_t,
    NONE                                = 0,
    FIELD_PICTURE                       = NriBit(0),
    INTRA                               = NriBit(1),
    IDR                                 = NriBit(2),
    BOTTOM_FIELD                        = NriBit(3),
    REFERENCE                           = NriBit(4),
    COMPLEMENTARY_FIELD_PAIR            = NriBit(5)
);

NriBits(VideoH265DecodePictureBits, uint8_t,
    NONE                                = 0,
    IRAP                                = NriBit(0),
    IDR                                 = NriBit(1),
    REFERENCE                           = NriBit(2),
    SHORT_TERM_REF_PIC_SET_SPS          = NriBit(3)
);

NriEnum(VideoEncodeFrameType, uint8_t,
    IDR,
    I,
    P,
    B
);

NriEnum(VideoEncodeRateControlMode, uint8_t,
    CQP
);

NriEnum(VideoAV1ReferenceName, uint8_t,
    NONE,
    LAST,
    LAST2,
    LAST3,
    GOLDEN,
    BWDREF,
    ALTREF2,
    ALTREF
);

NriBits(VideoAV1SequenceBits, uint32_t,
    NONE                                = 0,
    STILL_PICTURE                       = NriBit(0),
    REDUCED_STILL_PICTURE_HEADER        = NriBit(1),
    USE_128X128_SUPERBLOCK              = NriBit(2),
    ENABLE_FILTER_INTRA                 = NriBit(3),
    ENABLE_INTRA_EDGE_FILTER            = NriBit(4),
    ENABLE_INTERINTRA_COMPOUND          = NriBit(5),
    ENABLE_MASKED_COMPOUND              = NriBit(6),
    ENABLE_WARPED_MOTION                = NriBit(7),
    ENABLE_DUAL_FILTER                  = NriBit(8),
    ENABLE_ORDER_HINT                   = NriBit(9),
    ENABLE_JNT_COMP                     = NriBit(10),
    ENABLE_REF_FRAME_MVS                = NriBit(11),
    FRAME_ID_NUMBERS_PRESENT            = NriBit(12),
    ENABLE_SUPERRES                     = NriBit(13),
    ENABLE_CDEF                         = NriBit(14),
    ENABLE_RESTORATION                  = NriBit(15),
    FILM_GRAIN_PARAMS_PRESENT           = NriBit(16),
    TIMING_INFO_PRESENT                 = NriBit(17),
    INITIAL_DISPLAY_DELAY_PRESENT       = NriBit(18),
    MONO_CHROME                         = NriBit(19),
    COLOR_RANGE                         = NriBit(20),
    SEPARATE_UV_DELTA_Q                 = NriBit(21),
    COLOR_DESCRIPTION_PRESENT           = NriBit(22)
);

NriBits(VideoAV1PictureBits, uint32_t,
    NONE                                = 0,
    ERROR_RESILIENT_MODE                = NriBit(0),
    DISABLE_CDF_UPDATE                  = NriBit(1),
    USE_SUPERRES                        = NriBit(2),
    RENDER_AND_FRAME_SIZE_DIFFERENT     = NriBit(3),
    ALLOW_SCREEN_CONTENT_TOOLS          = NriBit(4),
    IS_FILTER_SWITCHABLE                = NriBit(5),
    FORCE_INTEGER_MV                    = NriBit(6),
    FRAME_SIZE_OVERRIDE                 = NriBit(7),
    BUFFER_REMOVAL_TIME_PRESENT         = NriBit(8),
    ALLOW_INTRABC                       = NriBit(9),
    FRAME_REFS_SHORT_SIGNALING          = NriBit(10),
    ALLOW_HIGH_PRECISION_MV             = NriBit(11),
    IS_MOTION_MODE_SWITCHABLE           = NriBit(12),
    USE_REF_FRAME_MVS                   = NriBit(13),
    DISABLE_FRAME_END_UPDATE_CDF        = NriBit(14),
    ALLOW_WARPED_MOTION                 = NriBit(15),
    REDUCED_TX_SET                      = NriBit(16),
    REFERENCE_SELECT                    = NriBit(17),
    SKIP_MODE_PRESENT                   = NriBit(18),
    DELTA_Q_PRESENT                     = NriBit(19),
    DELTA_LF_PRESENT                    = NriBit(20),
    DELTA_LF_MULTI                      = NriBit(21),
    SEGMENTATION_ENABLED                = NriBit(22),
    SEGMENTATION_UPDATE_MAP             = NriBit(23),
    SEGMENTATION_TEMPORAL_UPDATE        = NriBit(24),
    SEGMENTATION_UPDATE_DATA            = NriBit(25),
    USES_LR                             = NriBit(26),
    USES_CHROMA_LR                      = NriBit(27),
    SHOW_FRAME                          = NriBit(28),
    SHOWABLE_FRAME                      = NriBit(29),
    APPLY_GRAIN                         = NriBit(30)
);

NriStruct(VideoSessionDesc) {
    Nri(VideoUsage) usage;
    Nri(VideoCodec) codec;
    Nri(Format) format;
    uint32_t width;
    uint32_t height;
    uint32_t maxReferenceNum;
};

NriStruct(VideoReference) {
    NriPtr(VideoPicture) picture;
    uint32_t slot;
};

NriStruct(VideoPictureDesc) {
    NriPtr(Texture) texture;
    Nri(Format) format;
    uint32_t subresource;
    uint32_t layer;
    uint32_t width;
    uint32_t height;
};

NriStruct(VideoH264SequenceParameterSetDesc) {
    Nri(VideoH264SequenceParameterSetBits) flags;
    uint8_t profileIdc;
    uint8_t levelIdc;
    uint8_t chromaFormatIdc;
    uint8_t sequenceParameterSetId;
    uint8_t bitDepthLumaMinus8;
    uint8_t bitDepthChromaMinus8;
    uint8_t log2MaxFrameNumMinus4;
    uint8_t pictureOrderCountType;
    int32_t offsetForNonReferencePicture;
    int32_t offsetForTopToBottomField;
    uint8_t log2MaxPictureOrderCountLsbMinus4;
    uint8_t referenceFrameNum;
    uint16_t pictureWidthInMbsMinus1;
    uint16_t pictureHeightInMapUnitsMinus1;
};

NriStruct(VideoH264PictureParameterSetDesc) {
    Nri(VideoH264PictureParameterSetBits) flags;
    uint8_t sequenceParameterSetId;
    uint8_t pictureParameterSetId;
    uint8_t refIndexL0DefaultActiveMinus1;
    uint8_t refIndexL1DefaultActiveMinus1;
    uint8_t weightedBipredIdc;
    int8_t pictureInitQpMinus26;
    int8_t pictureInitQsMinus26;
    int8_t chromaQpIndexOffset;
    int8_t secondChromaQpIndexOffset;
};

NriStruct(VideoH264SessionParametersDesc) {
    NriOptional const NriPtr(VideoH264SequenceParameterSetDesc) sequenceParameterSets; // if provided, must include "sequenceParameterSetNum" entries
    uint32_t sequenceParameterSetNum;
    NriOptional const NriPtr(VideoH264PictureParameterSetDesc) pictureParameterSets; // if provided, must include "pictureParameterSetNum" entries
    uint32_t pictureParameterSetNum;
    NriOptional uint32_t maxSequenceParameterSetNum; // defaults to "sequenceParameterSetNum"
    NriOptional uint32_t maxPictureParameterSetNum; // defaults to "pictureParameterSetNum"
};

NriStruct(VideoAV1SequenceDesc) {
    Nri(VideoAV1SequenceBits) flags;
    uint8_t seqProfile;
    uint8_t bitDepth;
    uint8_t subsamplingX;
    uint8_t subsamplingY;
    uint16_t maxFrameWidthMinus1;
    uint16_t maxFrameHeightMinus1;
    uint8_t frameWidthBitsMinus1;
    uint8_t frameHeightBitsMinus1;
    uint8_t deltaFrameIdLengthMinus2;
    uint8_t additionalFrameIdLengthMinus1;
    uint8_t orderHintBitsMinus1;
    uint8_t seqForceIntegerMv;
    uint8_t seqForceScreenContentTools;
    uint8_t level;
    uint8_t colorPrimaries;
    uint8_t transferCharacteristics;
    uint8_t matrixCoefficients;
    uint8_t chromaSamplePosition;
    uint32_t numUnitsInDisplayTick;
    uint32_t timeScale;
    uint32_t numTicksPerPictureMinus1;
};

NriStruct(VideoAV1SessionParametersDesc) {
    Nri(VideoAV1SequenceDesc) sequence;
};

NriStruct(VideoSessionParametersDesc) {
    NriPtr(VideoSession) session;
    NriOptional const NriPtr(VideoH264SessionParametersDesc) h264Parameters;
    NriOptional const NriPtr(VideoAV1SessionParametersDesc) av1Parameters;
};

NriStruct(VideoDecodeArgument) {
    Nri(VideoDecodeArgumentType) type;
    uint32_t size;
    const void* data;
};

NriStruct(VideoH264ReferenceDesc) {
    Nri(VideoEncodeFrameType) frameType;
    uint8_t temporalLayer;
    uint8_t listIndex;
    uint8_t longTermReference;
    uint32_t frameNum;
    int32_t pictureOrderCount;
    uint32_t slot;
    uint16_t longTermPictureIndex;
    uint16_t longTermFrameIndex;
};

NriStruct(VideoH264DecodePictureDesc) {
    Nri(VideoH264DecodePictureBits) flags;
    uint8_t sequenceParameterSetId;
    uint8_t pictureParameterSetId;
    uint16_t frameNum;
    uint16_t idrPictureId;
    int32_t topFieldOrderCount;
    int32_t bottomFieldOrderCount;
    NriOptional const uint32_t* sliceOffsets; // if provided, must include "sliceOffsetNum" entries
    uint32_t sliceOffsetNum;
    NriOptional const NriPtr(VideoH264ReferenceDesc) references; // if provided, must include "referenceNum" entries
    uint32_t referenceNum;
};

NriStruct(VideoH265ReferenceDesc) {
    uint32_t slot;
    int32_t pictureOrderCount;
    uint8_t temporalLayer;
    Nri(VideoEncodeFrameType) frameType;
    uint8_t longTerm;
    uint8_t listIndex;
};

NriStruct(VideoH265DecodePictureDesc) {
    Nri(VideoH265DecodePictureBits) flags;
    uint8_t videoParameterSetId;
    uint8_t sequenceParameterSetId;
    uint8_t pictureParameterSetId;
    int32_t pictureOrderCount;
    uint8_t numDeltaPocsOfRefRpsIdx;
    uint8_t reserved;
    uint16_t numBitsForShortTermRefPicSetInSlice;
    NriOptional const uint32_t* sliceSegmentOffsets; // if provided, must include "sliceSegmentOffsetNum" entries
    uint32_t sliceSegmentOffsetNum;
    NriOptional const NriPtr(VideoH265ReferenceDesc) references; // if provided, must include "referenceNum" entries
    uint32_t referenceNum;
};

NriStruct(VideoEncodeRateControlDesc) {
    Nri(VideoEncodeRateControlMode) mode;
    uint8_t qpI;
    uint8_t qpP;
    uint8_t qpB;
    uint32_t frameRateNumerator;
    uint32_t frameRateDenominator;
};

NriStruct(VideoEncodePictureDesc) {
    Nri(VideoEncodeFrameType) frameType;
    uint8_t temporalLayer;
    uint16_t idrPictureId;
    uint32_t frameIndex;
    int32_t pictureOrderCount;
};

NriStruct(VideoH264PictureDesc) {
    uint8_t sequenceParameterSetId;
    uint8_t pictureParameterSetId;
    uint16_t reserved;
    NriOptional const NriPtr(VideoH264ReferenceDesc) references; // if provided, must include "referenceNum" entries
    uint32_t referenceNum;
};

NriStruct(VideoAV1ReferenceDesc) {
    Nri(VideoAV1ReferenceName) name; // NONE describes an AV1 DPB slot that is not mapped to a current-frame reference name
    uint8_t refFrameIndex;
    Nri(VideoEncodeFrameType) frameType;
    uint8_t orderHint;
    uint32_t frameId;
    uint32_t slot;
};

NriStruct(VideoAV1TileLayoutDesc) {
    uint8_t columnNum;
    uint8_t rowNum;
    uint8_t tileSizeBytesMinus1;
    uint8_t uniformSpacing;
    uint16_t contextUpdateTileId;
    uint16_t reserved;
    NriOptional const uint16_t* miColumnStarts; // if provided, must include "columnNum + 1" entries
    NriOptional const uint16_t* miRowStarts; // if provided, must include "rowNum + 1" entries
    NriOptional const uint16_t* widthInSuperblocksMinus1; // if provided, must include "columnNum" entries
    NriOptional const uint16_t* heightInSuperblocksMinus1; // if provided, must include "rowNum" entries
};

NriStruct(VideoAV1PictureDesc) {
    uint32_t currentFrameId;
    uint8_t orderHint;
    uint8_t refreshFrameFlags;
    Nri(VideoAV1ReferenceName) primaryReferenceName;
    Nri(VideoAV1PictureBits) flags;
    uint16_t renderWidthMinus1;
    uint16_t renderHeightMinus1;
    uint8_t codedDenom;
    uint8_t interpolationFilter;
    uint8_t txMode;
    uint8_t baseQIndex;
    uint8_t cdefDampingMinus3;
    uint8_t cdefBits;
    uint8_t deltaQRes;
    uint8_t deltaLfRes;
    NriOptional const NriPtr(VideoAV1TileLayoutDesc) tileLayout;
    NriOptional const NriPtr(VideoAV1ReferenceDesc) references; // if provided, must include "referenceNum" DPB snapshot entries
    uint32_t referenceNum;
};

NriStruct(VideoAV1DecodeTileDesc) {
    uint32_t offset;
    uint32_t size;
    uint16_t row;
    uint16_t column;
    uint8_t anchorFrame;
    uint8_t reserved[3];
};

NriStruct(VideoAV1DecodePictureDesc) {
    Nri(VideoEncodeFrameType) frameType;
    uint8_t orderHint;
    uint8_t refreshFrameFlags;
    Nri(VideoAV1ReferenceName) primaryReferenceName;
    uint32_t currentFrameId;
    uint32_t frameHeaderOffset;
    Nri(VideoAV1PictureBits) flags;
    uint16_t renderWidthMinus1;
    uint16_t renderHeightMinus1;
    uint8_t baseQIndex;
    uint8_t superresDenom;
    uint8_t codedDenom;
    uint8_t interpolationFilter;
    uint8_t txMode;
    uint8_t cdefDampingMinus3;
    uint8_t cdefBits;
    uint8_t deltaQRes;
    uint8_t deltaLfRes;
    NriOptional const NriPtr(VideoAV1TileLayoutDesc) tileLayout;
    NriOptional const NriPtr(VideoAV1DecodeTileDesc) tiles; // if provided, must include "tileNum" entries
    uint32_t tileNum;
    NriOptional const NriPtr(VideoAV1ReferenceDesc) references; // if provided, must include "referenceNum" DPB snapshot entries
    uint32_t referenceNum;
};

NriStruct(VideoDecodeDesc) {
    NriPtr(VideoSession) session;
    NriOptional NriPtr(VideoSessionParameters) parameters;
    NriPtr(Buffer) bitstream;
    uint64_t bitstreamOffset;
    uint64_t bitstreamSize;
    NriPtr(VideoPicture) dstPicture;
    NriOptional const NriPtr(VideoReference) references; // if provided, must include "referenceNum" entries
    uint32_t referenceNum;
    uint32_t dstSlot;
    NriOptional const NriPtr(VideoDecodeArgument) arguments; // if provided, must include "argumentNum" entries
    uint32_t argumentNum;
    NriOptional const NriPtr(VideoH264DecodePictureDesc) h264PictureDesc;
    NriOptional const NriPtr(VideoH265DecodePictureDesc) h265PictureDesc;
    NriOptional const NriPtr(VideoAV1DecodePictureDesc) av1PictureDesc;
};

NriStruct(VideoEncodeDesc) {
    NriPtr(VideoSession) session;
    NriOptional NriPtr(VideoSessionParameters) parameters;
    NriPtr(VideoPicture) srcPicture;
    NriPtr(Buffer) dstBitstream;
    uint64_t dstBitstreamOffset;
    uint64_t bitstreamMetadataSize; // D3D12: bytes of codec metadata already written before the current frame payload
    NriOptional const NriPtr(VideoEncodePictureDesc) pictureDesc;
    NriOptional const NriPtr(VideoEncodeRateControlDesc) rateControlDesc;
    NriOptional NriPtr(VideoPicture) reconstructedPicture;
    NriOptional NriPtr(Buffer) metadata;
    uint64_t metadataOffset;
    NriOptional NriPtr(Buffer) resolvedMetadata;
    uint64_t resolvedMetadataOffset;
    NriOptional const NriPtr(VideoReference) references; // if provided, must include "referenceNum" entries
    uint32_t referenceNum;
    uint32_t reconstructedSlot;
    NriOptional const NriPtr(VideoH264PictureDesc) h264PictureDesc;
    NriOptional const NriPtr(VideoAV1PictureDesc) av1PictureDesc;
    NriOptional const NriPtr(VideoH265ReferenceDesc) h265ReferenceDescs; // if provided, must include "referenceNum" entries
};

// Threadsafe: no
NriStruct(VideoInterface) {
    // Session
    // {
        Nri(Result) (NRI_CALL *CreateVideoSession)  (NriRef(Device) device, const NriRef(VideoSessionDesc) videoSessionDesc, NriOut NriRef(VideoSession*) videoSession);
        void        (NRI_CALL *DestroyVideoSession) (NriRef(VideoSession) videoSession);
        Nri(Result) (NRI_CALL *CreateVideoSessionParameters)  (NriRef(Device) device, const NriRef(VideoSessionParametersDesc) videoSessionParametersDesc, NriOut NriRef(VideoSessionParameters*) videoSessionParameters);
        void        (NRI_CALL *DestroyVideoSessionParameters) (NriRef(VideoSessionParameters) videoSessionParameters);
        Nri(Result) (NRI_CALL *CreateVideoPicture)  (NriRef(Device) device, const NriRef(VideoPictureDesc) videoPictureDesc, NriOut NriRef(VideoPicture*) videoPicture);
        void        (NRI_CALL *DestroyVideoPicture) (NriRef(VideoPicture) videoPicture);
    // }

    // Command buffer
    // {
        // Video decode/encode command buffers must be created from "QueueType::VIDEO_DECODE" or "QueueType::VIDEO_ENCODE" queues.
        void (NRI_CALL *CmdDecodeVideo) (NriRef(CommandBuffer) commandBuffer, const NriRef(VideoDecodeDesc) videoDecodeDesc);
        void (NRI_CALL *CmdEncodeVideo) (NriRef(CommandBuffer) commandBuffer, const NriRef(VideoEncodeDesc) videoEncodeDesc);
    // }
};

NriNamespaceEnd
