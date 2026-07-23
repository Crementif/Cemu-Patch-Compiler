
; =============================================================================
;  Stereo Instancing - Core Patch
; =============================================================================


; -----------------------------------------------------------------------------
; Placeholder hooks for hardware stereo instancing.
;
; Hook targets (from re-notes):
;   GX2DrawIndexedEx          — Redirect to instanced draw with instanceCount=2
;   Fetch shader setup        — Set attribute divisor=1 for eye index
;   PA_SU_SC_MODE_CNTL        — Enable VGT_MULTI_PRIM_IB_INDEXED_EN
;   Uniform block binds       — Inject eye matrix/offset uniforms
; -----------------------------------------------------------------------------
