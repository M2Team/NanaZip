// SplitUtils.h

#ifndef __SPLIT_UTILS_H
#define __SPLIT_UTILS_H

#include "../../../Common/MyTypes.h"
#include "../../../Common/MyString.h"

#include "../../../Windows/Control/ComboBox.h"

bool ParseVolumeSizes(const UString &s, CRecordVector<UInt64> &values);
void AddVolumeItems(NWindows::NControl::CComboBox &volumeCombo);
UInt64 GetNumberOfVolumes(UInt64 size, const CRecordVector<UInt64> &volSizes);

#endif
