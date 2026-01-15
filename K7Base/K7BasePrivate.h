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
 * @brief Initializes the NanaZip policy settings.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 * @remarks The function should be called only once during the K7Base library
 *          initialization phase as early as possible.
 */
EXTERN_C MO_RESULT MOAPI K7BaseInitializePolicies();

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
 * @brief Enables the mandatory mitigations for the current process.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseEnableMandatoryMitigations();

/**
 * @brief Disables dynamic code generation for the current process if the policy
 *        is set to disallow it.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseDisableDynamicCodeGeneration();

/**
 * @brief Disables child process creation for the current process if the policy
 *        is set to disallow it.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseDisableChildProcessCreation();

/**
 * @brief Sets the dynamic code generation policy for the current thread.
 * @param AllowDynamicCodeGeneration Indicates whether to allow dynamic code
 *                                   generation for the current thread.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseSetCurrentThreadDynamicCodePolicyOptOut(
    _In_ MO_BOOL AllowDynamicCodeGeneration);

/**
 * @brief Initializes the dynamic link library blocker to workaround various
 *        issues caused by incompatible dynamic link libraries.
 * @return If the function succeeds, it returns MO_RESULT_SUCCESS_OK. Otherwise,
 *         it returns an MO_RESULT error code.
 */
EXTERN_C MO_RESULT MOAPI K7BaseInitializeDynamicLinkLibraryBlocker();

#endif // !K7_BASE_MITIGATIONS_PRIVATE

#endif // !K7_BASE_PRIVATE
