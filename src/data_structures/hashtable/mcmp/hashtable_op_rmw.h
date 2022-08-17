#ifndef CACHEGRAND_HASHTABLE_OP_RMW_H
#define CACHEGRAND_HASHTABLE_OP_RMW_H

#ifdef __cplusplus
extern "C" {
#endif

bool hashtable_mcmp_op_rmw_begin(
        hashtable_t *hashtable,
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction,
        hashtable_key_data_t *key,
        hashtable_key_size_t key_size,
        hashtable_value_data_t *current_value);

void hashtable_mcmp_op_rmw_commit_update(
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction,
        hashtable_value_data_t new_value);

void hashtable_mcmp_op_rmw_commit_delete(
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction);

void hashtable_mcmp_op_rmw_abort(
        hashtable_mcmp_op_rmw_transaction_t *rmw_transaction);

#ifdef __cplusplus
}
#endif

#endif //CACHEGRAND_HASHTABLE_OP_RMW_H
