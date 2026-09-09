SECTION code_user

EXTERN _spectranext_time_sync_ovl

EXTERN _spectranext_time_shift_ovl

    DEFB 2
    DW _spectranext_time_sync_ovl
    DW _spectranext_time_shift_ovl
