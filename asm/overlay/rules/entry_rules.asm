SECTION code_user

EXTERN _rules_play_ovl
EXTERN _rules_check_ovl
EXTERN _rules_hints_ovl
EXTERN _rules_hints_clear_ovl
EXTERN _rules_restore_validate_ovl

    DEFB 5
    DW _rules_play_ovl
    DW _rules_check_ovl
    DW _rules_hints_ovl
    DW _rules_hints_clear_ovl
    DW _rules_restore_validate_ovl
