/*
 * PROJECT:    NanaZip Platform Base Library (K7Base)
 * FILE:       K7BasePrivate.h
 * PURPOSE:    Definition for NanaZip Platform Base Private Interfaces
 *
 * LICENSE:    The MIT License
 *
 * MAINTAINER: MouriNaruto (Kenji.Mouri@outlook.com)
 */

#ifndef K7_BASE_PRIVATE
#define K7_BASE_PRIVATE

#include "K7Base.h"

#ifndef K7_BASE_POLICIES_PRIVATE
#define K7_BASE_POLICIES_PRIVATE

/**
 * @brief Retrieves the policy setting for allowing dynamic code generation.
 * @return Returns MO_TRUE if dynamic code generation is allowed, or MO_FALSE if
 *         not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowDynamicCodeGenerationPolicy();

/**
 * @brief Retrieves the policy setting for allowing child process creation.
 * @return Returns MO_TRUE if child process creation is allowed, or MO_FALSE if
 *         not.
 */
EXTERN_C MO_BOOL MOAPI K7BaseGetAllowChildProcessCreationPolicy();

#endif // !K7_BASE_POLICIES_PRIVATE

#ifndef K7_BASE_MITIGATIONS_PRIVATE
#define K7_BASE_MITIGATIONS_PRIVATE

/**
 * @brief Sets the dynamic code generation policy for the current thread.
 * @param AllowDynamicCodeGeneration Indicates whether to allow dynamic code
 *                                   generation for the current thread.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseSetCurrentThreadDynamicCodePolicyOptOut(
    _In_ MO_BOOL AllowDynamicCodeGeneration);

#endif // !K7_BASE_MITIGATIONS_PRIVATE

#endif // !K7_BASE_PRIVATE
