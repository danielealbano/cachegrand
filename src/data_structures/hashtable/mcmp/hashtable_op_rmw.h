#ifndef CACHEGRAND_HASHTABLE_OP_RMW_H
#define CACHEGRAND_HASHTABLE_OP_RMW_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_rmw_begin(
        hashtable_t *hashtable,
        transaction_t *transaction,
        hashtable_mcmp_op_rmw_status_t *rmw_status,
        hashtable_database_number_t database_number,
        hashtable_key_data_t *key,
        hashtable_key_length_t key_length,
        hashtable_value_data_t *current_value);

void hashtable_mcmp_op_rmw_commit_update(
        hashtable_mcmp_op_rmw_status_t *rmw_status,
        hashtable_value_data_t new_value);

void hashtable_mcmp_op_rmw_commit_delete(
        hashtable_mcmp_op_rmw_status_t *rmw_status);

void hashtable_mcmp_op_rmw_abort(
        hashtable_mcmp_op_rmw_status_t *rmw_status);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_RMW_H
