/* Shared host/overlay restore validation. The caller commits separately so a
   rejected snapshot cannot alter the active game. */
int ms_rules_validate_restore(const ms_state_t *in)
{
    uint8_t i;

    if (in == 0) {
        return MS_ERR_NULL;
    }
    if (in->side > MS_SIDE_B || in->over > 1u) {
        return MS_ERR_ILLEGAL;
    }
    for (i = 0u; i < MS_CELLS; ++i) {
        if (in->cells[i] != MS_CELL_EMPTY &&
            in->cells[i] != MS_CELL_A && in->cells[i] != MS_CELL_B) {
            return MS_ERR_ILLEGAL;
        }
    }
    if (in->over == 0u) {
        if (ms_rules_cells_side_has_moves(in->cells, in->side) == 0u) {
            return MS_ERR_ILLEGAL;
        }
    } else if (ms_rules_cells_side_has_moves(in->cells, in->side) != 0u ||
               ms_rules_cells_side_has_moves(
                   in->cells, (uint8_t)(in->side ^ 1u)) != 0u) {
        return MS_ERR_ILLEGAL;
    }
    return MS_OK;
}
