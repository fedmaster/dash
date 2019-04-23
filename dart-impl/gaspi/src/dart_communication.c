#include <string.h>

#include <dash/dart/gaspi/dart_gaspi.h>
#include <dash/dart/gaspi/gaspi_utils.h>
#include <dash/dart/gaspi/dart_team_private.h>
#include <dash/dart/gaspi/dart_translation.h>
#include <dash/dart/gaspi/dart_mem.h>
#include <dash/dart/gaspi/dart_communication_priv.h>



/*********************** Notify Value ************************/
//gaspi_notification_t dart_notify_value = 42;


/********************** Only for testing *********************/
gaspi_queue_id_t dart_handle_get_queue(dart_handle_t handle)
{
    return handle->queue;
}

dart_ret_t dart_scatter(
   const void        * sendbuf,
   void              * recvbuf,
   size_t              nelem,
   dart_datatype_t     dtype,
   dart_team_unit_t    root,
   dart_team_t         teamid)
{
    dart_team_unit_t             myid;
    size_t                  team_size;
    gaspi_notification_id_t first_id;
    gaspi_notification_t    old_value;
    gaspi_segment_id_t      gaspi_seg_id = dart_gaspi_buffer_id;
    gaspi_notification_id_t remote_id    = 0;
    gaspi_pointer_t         seg_ptr      = NULL;
    gaspi_queue_id_t        queue        = 0;
    gaspi_offset_t          local_offset = 0;
    uint16_t                index;
    size_t                  nbytes = dart_gaspi_datatype_sizeof(dtype) * nelem;

    DART_CHECK_ERROR(dart_team_myid(teamid, &myid));
    DART_CHECK_ERROR(dart_team_size(teamid, &team_size));

    if(dart_adapt_teamlist_convert(teamid, &index) == -1)
    {
        fprintf(stderr, "dart_scatter: can't find index of given team\n");
        return DART_ERR_OTHER;
    }

    if((nbytes * team_size) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_create(dart_fallback_seg,
                                                    nbytes * team_size,
                                                    dart_teams[index].id,
                                                    GASPI_BLOCK,
                                                    GASPI_MEM_UNINITIALIZED));
        gaspi_seg_id = dart_fallback_seg;
        dart_fallback_seg_is_allocated = true;
    }

    DART_CHECK_ERROR(dart_barrier(teamid));
    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));

    if(myid.id == root.id)
    {
        memcpy( seg_ptr, sendbuf, nbytes * team_size );

        for (dart_unit_t unit = 0; unit < team_size; ++unit)
        {
            if ( unit == myid.id )
            {
                continue;
            }

            local_offset = nbytes * unit;
            dart_unit_t unit_abs;

            DART_CHECK_ERROR(unit_l2g(index, &unit_abs, unit));
            DART_CHECK_GASPI_ERROR(wait_for_queue_entries(&queue, 2));

            DART_CHECK_GASPI_ERROR(gaspi_write_notify(gaspi_seg_id,
                                                      local_offset,
                                                      unit_abs,
                                                      gaspi_seg_id,
                                                      0UL,
                                                      nbytes,
                                                      remote_id,
                                                      42,
                                                      queue,
                                                      GASPI_BLOCK));
        }

        memcpy(recvbuf, (void *) ((char *) seg_ptr + (myid.id * nbytes)), nbytes);
    }
    else
    {
        DART_CHECK_GASPI_ERROR(gaspi_notify_waitsome(gaspi_seg_id, remote_id, 1, &first_id, GASPI_BLOCK));
        DART_CHECK_GASPI_ERROR(gaspi_notify_reset(gaspi_seg_id, first_id, &old_value));

        memcpy(recvbuf, seg_ptr, nbytes);
    }

    DART_CHECK_ERROR(dart_barrier(teamid));

    if((nbytes * team_size) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_delete(gaspi_seg_id));
        dart_fallback_seg_is_allocated = false;
    }
    return DART_OK;
}

//void *sendbuf, void *recvbuf, size_t nbytes, dart_unit_t root, dart_team_t team
dart_ret_t dart_gather(
      const void         * sendbuf,
      void               * recvbuf,
      size_t               nelem,
      dart_datatype_t      dtype,
      dart_team_unit_t     root,
      dart_team_t          teamid)
{
    uint16_t                index;
    dart_team_unit_t        relative_id;
    size_t                  team_size;
    gaspi_notification_id_t first_id;
    gaspi_notification_t    old_value;
    gaspi_segment_id_t      gaspi_seg_id  = dart_gaspi_buffer_id;
    gaspi_notification_t    notify_value  = 42;
    gaspi_pointer_t         seg_ptr       = NULL;
    gaspi_queue_id_t        queue         = 0;
    gaspi_offset_t          remote_offset = 0;
    size_t                  nbytes = dart_gaspi_datatype_sizeof(dtype) * nelem;

    if(dart_adapt_teamlist_convert(teamid, &index) == -1)
    {
        fprintf(stderr, "dart_gather: no team with id: %d\n", teamid);
        return DART_ERR_OTHER;
    }

    DART_CHECK_ERROR(dart_team_myid(teamid, &relative_id));
    DART_CHECK_ERROR(dart_team_size(teamid, &team_size));

    if((nbytes * team_size) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_create(dart_fallback_seg,
                                                    nbytes * team_size,
                                                    dart_teams[index].id,
                                                    GASPI_BLOCK,
                                                    GASPI_MEM_UNINITIALIZED));
        gaspi_seg_id = dart_fallback_seg;
        dart_fallback_seg_is_allocated = true;
    }

    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));

    DART_CHECK_ERROR(dart_barrier(teamid));

    if(relative_id.id != root.id)
    {
        dart_unit_t abs_root_id;
        DART_CHECK_ERROR(unit_l2g(index, &abs_root_id, root.id));

        memcpy(seg_ptr, sendbuf, nbytes);
        remote_offset = relative_id.id * nbytes;

        DART_CHECK_GASPI_ERROR(wait_for_queue_entries(&queue, 2));
        DART_CHECK_GASPI_ERROR(gaspi_write_notify(gaspi_seg_id,
                                                  0UL,
                                                  abs_root_id,
                                                  gaspi_seg_id,
                                                  remote_offset,
                                                  nbytes,
                                                  relative_id.id,
                                                  notify_value,
                                                  queue,
                                                  GASPI_BLOCK));
    }
    else
    {
        gaspi_offset_t recv_buffer_offset = relative_id.id * nbytes;
        void         * recv_buffer_ptr    = (void *)((char *) seg_ptr + recv_buffer_offset);
        memcpy(recv_buffer_ptr, sendbuf, nbytes);

        int missing = team_size - 1;
        while(missing-- > 0)
        {
            DART_CHECK_GASPI_ERROR(blocking_waitsome(0, team_size, &first_id, &old_value, gaspi_seg_id));
            if(old_value != notify_value)
            {
                fprintf(stderr, "dart_gather: Error in process synchronization\n");
                break;
            }
        }
        memcpy(recvbuf, seg_ptr, team_size * nbytes);
    }

    DART_CHECK_ERROR(dart_barrier(teamid));

    if((nbytes * team_size) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_delete(gaspi_seg_id));
        dart_fallback_seg_is_allocated = false;
    }

    return DART_OK;
}
/**
 * Implemented a binominal tree to broadcast the data
 *
 * TODO : In error case memory of children will leak
 * TODO : check if nbytes is the actuall number of bytes
 */
 //void *buf, size_t nbytes, dart_unit_t root, dart_team_t team
dart_ret_t dart_bcast(
   void              * buf,
   size_t              nelem,
   dart_datatype_t     dtype,
   dart_team_unit_t    root,
   dart_team_t         teamid
)
{
    const gaspi_notification_id_t notify_id    = 0;
    gaspi_queue_id_t              queue        = 0;
    gaspi_pointer_t               seg_ptr      = NULL;
    const gaspi_notification_t    notify_val   = 42;
    gaspi_segment_id_t            gaspi_seg_id = dart_gaspi_buffer_id;
    gaspi_notification_id_t       first_id;
    gaspi_notification_t          old_value;
    uint16_t                      index;
    dart_global_unit_t            myid;
    dart_unit_t                   root_abs;
    dart_team_unit_t              team_myid;
    size_t                        team_size;
    int                           parent;
    int                         * children = NULL;
    int                           children_count;
    size_t                  nbytes = dart_gaspi_datatype_sizeof(dtype) * nelem;
    int result = dart_adapt_teamlist_convert(teamid, &index);
    if(result == -1)
    {
        fprintf(stderr, "dart_bcast: can't find index of given team\n");
        return DART_ERR_INVAL;
    }

    DART_CHECK_ERROR(unit_l2g(index, &root_abs, root.id));
    DART_CHECK_ERROR(dart_myid(&myid));
    DART_CHECK_ERROR(dart_team_myid(teamid, &team_myid));
    DART_CHECK_ERROR(dart_team_size(teamid, &team_size));
    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));

    if(nbytes > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_create(dart_fallback_seg,
                                                    nbytes,
                                                    dart_teams[index].id,
                                                    GASPI_BLOCK,
                                                    GASPI_MEM_UNINITIALIZED));
        gaspi_seg_id = dart_fallback_seg;
        dart_fallback_seg_is_allocated = true;
    }

    if(myid.id == root_abs)
    {
        memcpy(seg_ptr, buf, nbytes);
    }

    children_count = gaspi_utils_compute_comms(&parent, &children, team_myid.id, root.id, team_size);

    DART_CHECK_ERROR(dart_barrier(teamid));

    dart_unit_t abs_parent;
    DART_CHECK_ERROR(unit_l2g(index, &abs_parent, parent));
    /*
     * parents + children wait for upper parents data
     */
    if (myid.id != abs_parent)
    {
        blocking_waitsome(notify_id, 1, &first_id, &old_value, gaspi_seg_id);
        if(old_value != notify_val)
        {
            fprintf(stderr, "dart_bcast : Got wrong notify value -> data transfer error\n");
        }
    }
    /*
     * write to all childs
     */
    for (int child = 0; child < children_count; child++)
    {
        dart_unit_t abs_child;
        DART_CHECK_ERROR(unit_l2g(index, &abs_child, children[child]));

        DART_CHECK_GASPI_ERROR(wait_for_queue_entries(&queue, 2));
        DART_CHECK_GASPI_ERROR(gaspi_write_notify(gaspi_seg_id,
                                                  0UL,
                                                  abs_child,
                                                  gaspi_seg_id,
                                                  0UL,
                                                  nbytes,
                                                  notify_id,
                                                  notify_val,
                                                  queue,
                                                  GASPI_BLOCK));
    }

    free(children);
    DART_CHECK_ERROR(dart_barrier(teamid));

    if(myid.id != root_abs)
    {
        memcpy(buf, seg_ptr, nbytes);
    }

    if(nbytes > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_delete(gaspi_seg_id));
        dart_fallback_seg_is_allocated = false;
    }

    return DART_OK;
}
//(void *sendbuf, void *recvbuf, size_t nbytes, dart_team_t team)
dart_ret_t dart_allgather(
  const void      * sendbuf,
  void            * recvbuf,
  size_t            nelem,
  dart_datatype_t   dtype,
  dart_team_t       teamid)
{
    gaspi_queue_id_t     queue = 0;
    gaspi_notification_t notify_value = 42;
    gaspi_segment_id_t   gaspi_seg_id = dart_gaspi_buffer_id;
    gaspi_pointer_t      seg_ptr      = NULL;
    dart_team_unit_t     relative_id;
    gaspi_offset_t       offset;
    size_t               teamsize;
    uint16_t             index;
    size_t               nbytes = dart_gaspi_datatype_sizeof(dtype) * nelem;

    DART_CHECK_ERROR(dart_team_myid(teamid, &relative_id));
    DART_CHECK_ERROR(dart_team_size(teamid, &teamsize));
    DART_CHECK_ERROR(dart_barrier(teamid));

    int result = dart_adapt_teamlist_convert(teamid, &index);
    if (result == -1)
    {
        return DART_ERR_INVAL;
    }

    if((nbytes * teamsize) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_create(dart_fallback_seg,
                                                    nbytes * teamsize,
                                                    dart_teams[index].id,
                                                    GASPI_BLOCK,
                                                    GASPI_MEM_UNINITIALIZED));
        gaspi_seg_id = dart_fallback_seg;
        dart_fallback_seg_is_allocated = true;
    }


    offset = nbytes * relative_id.id;

    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));

    memcpy((void *) ((char *)seg_ptr + offset), sendbuf, nbytes);

    for (dart_unit_t unit = 0; unit < teamsize; ++unit)
    {
        if(unit == relative_id.id) continue;

        dart_unit_t unit_abs_id;
        DART_CHECK_ERROR(unit_l2g(index, &unit_abs_id, unit));

        DART_CHECK_GASPI_ERROR(wait_for_queue_entries(&queue, 2));
        DART_CHECK_GASPI_ERROR(gaspi_write_notify(gaspi_seg_id,
                                                  offset,
                                                  unit_abs_id,
                                                  gaspi_seg_id,
                                                  offset,
                                                  nbytes,
                                                  (gaspi_notification_id_t) relative_id.id,
                                                  notify_value,
                                                  queue,
                                                  GASPI_BLOCK));
    }

    int missing = teamsize - 1;
    gaspi_notification_id_t id_available;
    gaspi_notification_t    id_val;

    while(missing-- > 0)
    {
        DART_CHECK_GASPI_ERROR(blocking_waitsome(0, teamsize, &id_available, &id_val, gaspi_seg_id));
        if(id_val != notify_value)
        {
            fprintf(stderr, "Error: Get wrong notify in allgather\n");
        }
    }

    memcpy(recvbuf, seg_ptr, nbytes * teamsize);
    DART_CHECK_ERROR(dart_barrier(teamid));

    if((nbytes * teamsize) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_delete(gaspi_seg_id));
        dart_fallback_seg_is_allocated = false;
    }

    return DART_OK;
}


//slightly altered version of gaspi dart_allgather
dart_ret_t dart_allgatherv(
  const void      * sendbuf,
  size_t            nsendelem,
  dart_datatype_t   dtype,
  void            * recvbuf,
  const size_t    * nrecvcounts,
  const size_t    * recvdispls,
  dart_team_t       teamid)
{
    gaspi_queue_id_t     queue = 0;
    gaspi_notification_t notify_value = 42;
    gaspi_segment_id_t   gaspi_seg_id = dart_gaspi_buffer_id;
    gaspi_pointer_t      seg_ptr      = NULL;
    dart_team_unit_t     relative_id;
    gaspi_offset_t       offset;
    size_t               teamsize;
    uint16_t             index;
    size_t               nbytes = dart_gaspi_datatype_sizeof(dtype) * nsendelem;

    DART_CHECK_ERROR(dart_team_myid(teamid, &relative_id));
    DART_CHECK_ERROR(dart_team_size(teamid, &teamsize));
    DART_CHECK_ERROR(dart_barrier(teamid));

    /*number of all allements
    *|data+disp of unit 0..n-1|disp for last(n-th) unit|data of last(n-th) unit|
    *                                                  ^-- pointer recvdispls[teamsize-1]
    *|----------------------------- num_overall_ellem -------------------------|
    */
    size_t num_overall_elemnts = recvdispls[teamsize-1] + nrecvcounts[teamsize-1];
    size_t n_total_bytes = dart_gaspi_datatype_sizeof(dtype) * num_overall_elemnts;

    int result = dart_adapt_teamlist_convert(teamid, &index);
    if (result == -1)
    {
        return DART_ERR_INVAL;
    }

    if((n_total_bytes * teamsize) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_create(dart_fallback_seg,
                                                    n_total_bytes * teamsize,
                                                    dart_teams[index].id,
                                                    GASPI_BLOCK,
                                                    GASPI_MEM_UNINITIALIZED));
        gaspi_seg_id = dart_fallback_seg;
        dart_fallback_seg_is_allocated = true;
    }

    //local offset in Bytes. copies data from sendbuff in local part of the
    //segment to avoid communication and send to other units.
    offset = nbytes * relative_id.id + recvdispls[relative_id.id-1] * \
             dart_gaspi_datatype_sizeof(dtype);

    DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));

    memcpy((void *) ((char *)seg_ptr + offset), sendbuf, nbytes);

    for (dart_unit_t unit = 0; unit < teamsize; ++unit)
    {
        //Avoid communication if target is this unit.
        if(unit == relative_id.id) continue;

        dart_unit_t unit_abs_id;
        DART_CHECK_ERROR(unit_l2g(index, &unit_abs_id, unit));

        DART_CHECK_GASPI_ERROR(wait_for_queue_entries(&queue, 2));
        DART_CHECK_GASPI_ERROR(gaspi_write_notify(gaspi_seg_id,
                                                  offset,
                                                  unit_abs_id,
                                                  gaspi_seg_id,
                                                  offset,
                                                  nbytes,
                                                  (gaspi_notification_id_t) relative_id.id,
                                                  notify_value,
                                                  queue,
                                                  GASPI_BLOCK));
    }

    int missing = teamsize - 1;
    gaspi_notification_id_t id_available;
    gaspi_notification_t    id_val;

    while(missing-- > 0)
    {
        DART_CHECK_GASPI_ERROR(blocking_waitsome(0, teamsize, &id_available, &id_val, gaspi_seg_id));
        if(id_val != notify_value)
        {
            fprintf(stderr, "Error: Get wrong notify in allgather\n");
        }
    }

    memcpy(recvbuf, seg_ptr, nbytes * teamsize);
    DART_CHECK_ERROR(dart_barrier(teamid));

    if((nbytes * teamsize) > DART_GASPI_BUFFER_SIZE)
    {
        DART_CHECK_GASPI_ERROR(gaspi_segment_delete(gaspi_seg_id));
        dart_fallback_seg_is_allocated = false;
    }

    return DART_OK;
}

dart_ret_t dart_barrier (
  dart_team_t teamid)
{
    uint16_t       index;
    if (dart_adapt_teamlist_convert (teamid, &index) == -1)
    {
        return DART_ERR_INVAL;
    }
    gaspi_group_t gaspi_group_id = dart_teams[index].id;
    DART_CHECK_GASPI_ERROR(gaspi_barrier(gaspi_group_id, GASPI_BLOCK));

    return DART_OK;
}


dart_ret_t dart_get_blocking(
  void            * dst,
  dart_gptr_t       gptr,
  size_t            nelem,
  dart_datatype_t   src_type,
  dart_datatype_t   dst_type)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    // initialized with relative team unit id
    dart_unit_t global_src_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_src_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_src_unit_id, &gaspi_src_seg_id, "dart_get_blocking"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_src_unit_id)
    {
        DART_CHECK_ERROR(local_get(&gptr, gaspi_src_seg_id, dst, &conv_type));
        return DART_OK;
    }

    gaspi_queue_id_t queue = (gaspi_queue_id_t) -1;

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_get(&gptr,
                   global_src_unit_id,
                   gaspi_src_seg_id,
                   dart_onesided_seg,
                   dst,
                   &queue,
                   &conv_type)
    );

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label, gaspi_wait(queue, GASPI_BLOCK));

    DART_CHECK_ERROR(gaspi_segment_delete(dart_onesided_seg));

    free_converted_type(&conv_type);

    return DART_OK;

dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(dart_onesided_seg));
    free_converted_type(&conv_type);

    return DART_ERR_OTHER;
}

dart_ret_t dart_put_blocking(
  dart_gptr_t       gptr,
  const void      * src,
  size_t            nelem,
  dart_datatype_t   src_type,
  dart_datatype_t   dst_type)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    // initialized with relative team unit id
    dart_unit_t global_dst_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_dst_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_dst_unit_id, &gaspi_dst_seg_id, "dart_put_handle"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_dst_unit_id)
    {
        DART_CHECK_ERROR(local_put(&gptr, gaspi_dst_seg_id, src, &conv_type));
        return DART_OK;
    }

    gaspi_queue_id_t queue = (gaspi_queue_id_t) -1;

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_put(&gptr,
                   global_dst_unit_id,
                   gaspi_dst_seg_id,
                   dart_onesided_seg,
                   src,
                   &queue,
                   &conv_type)
    );

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label, put_completion_test(global_dst_unit_id, queue));

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label, gaspi_wait(queue, GASPI_BLOCK));

    DART_CHECK_ERROR(gaspi_segment_delete(dart_onesided_seg));

    free_converted_type(&conv_type);

    return DART_OK;

dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(dart_onesided_seg));
    free_converted_type(&conv_type);

    return DART_ERR_OTHER;
}

dart_ret_t dart_handle_free(
  dart_handle_t * handleptr)
{
    dart_handle_t handle = *handleptr;
    gaspi_notification_t val = 0;
    DART_CHECK_GASPI_ERROR(gaspi_notify_reset (handle->local_seg_id, handle->local_seg_id, &val));
    if(handle->comm_kind == GASPI_WRITE)
    {
        gaspi_notification_t val_remote = 0;
        DART_CHECK_GASPI_ERROR(gaspi_notify_reset (handle->local_seg_id, handle->notify_remote, &val_remote));
        if(val_remote != handle->notify_remote)
        {
          DART_LOG_ERROR("Error: gaspi remote completion notify value != expected value");
        }
    }

    if(val != handle->local_seg_id)
    {
      DART_LOG_ERROR("Error: gaspi notify value != expected value");
    }

    DART_CHECK_GASPI_ERROR(gaspi_segment_delete(handle->local_seg_id));
    DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, handle->local_seg_id));

    free(handle);
    *handleptr = DART_HANDLE_NULL;

    return DART_OK;
}

dart_ret_t dart_wait_local (
  dart_handle_t * handleptr)
{
    if (handleptr != NULL && *handleptr != DART_HANDLE_NULL)
    {
        gaspi_wait((*handleptr)->queue, GASPI_BLOCK);

        DART_CHECK_ERROR(dart_handle_free(handleptr));
    }

    return DART_OK;
}

dart_ret_t dart_waitall_local(
  dart_handle_t handles[],
  size_t        num_handles)
{
    if (handles != NULL)
    {
        for(size_t i = 0 ; i < num_handles ; ++i)
        {
            DART_CHECK_ERROR(dart_wait_local(&handles[i]));
        }
    }

    return DART_OK;
}

dart_ret_t dart_wait(
  dart_handle_t * handleptr)
{

    if( handleptr != NULL && *handleptr != DART_HANDLE_NULL )
    {
        DART_CHECK_GASPI_ERROR((gaspi_wait((*handleptr)->queue, GASPI_BLOCK)));
        dart_handle_free(handleptr);
    }

    return DART_OK;
}

dart_ret_t dart_waitall(
  dart_handle_t handles[],
  size_t        num_handles)
{
    DART_LOG_DEBUG("dart_waitall()");
    if ( handles == NULL || num_handles == 0)
    {
        DART_LOG_DEBUG("dart_waitall: empty handles");
        return DART_OK;
    }

    for(size_t i = 0; i < num_handles; ++i)
    {
        DART_CHECK_GASPI_ERROR(gaspi_wait(handles[i]->queue, GASPI_BLOCK));
    }

    return DART_OK;
}

dart_ret_t dart_test_local (
  dart_handle_t * handleptr,
  int32_t       * is_finished)
{
    if (handleptr == NULL || *handleptr == DART_HANDLE_NULL)
    {
        *is_finished = 1;
        DART_LOG_DEBUG("dart_test_local: empty handle");

        return DART_OK;
    }

    return dart_test_impl(handleptr, is_finished, (*handleptr)->local_seg_id);
}

dart_ret_t dart_testall_local(
  dart_handle_t   handles[],
  size_t          num_handles,
  int32_t       * is_finished)
{
    if (handles == NULL || num_handles == 0)
    {
        *is_finished = 1;
        DART_LOG_DEBUG("dart_testall_local: empty handle");

        return DART_OK;
    }

    return dart_test_all_impl(handles, num_handles, is_finished, GASPI_LOCAL);
}

dart_ret_t dart_test(
  dart_handle_t * handleptr,
  int32_t       * is_finished)
{
    if(handleptr == NULL || *handleptr == DART_HANDLE_NULL)
    {
        *is_finished = 1;
        DART_LOG_DEBUG("dart_test: empty handle");

        return DART_OK;
    }

    if((*handleptr)->comm_kind == GASPI_READ)
    {
        return dart_test_impl(handleptr, is_finished, (*handleptr)->local_seg_id);
    }

    return dart_test_impl(handleptr, is_finished, (*handleptr)->notify_remote);
}

dart_ret_t dart_testall(
  dart_handle_t   handles[],
  size_t          num_handles,
  int32_t       * is_finished)
{
    if(handles == NULL || num_handles == 0)
    {
        *is_finished = 1;
        DART_LOG_DEBUG("dart_testall: empty handle");

        return DART_OK;
    }

    return dart_test_all_impl(handles, num_handles, is_finished, GASPI_GLOBAL);
}

dart_ret_t dart_get_handle(
  void*           dst,
  dart_gptr_t     gptr,
  size_t          nelem,
  dart_datatype_t src_type,
  dart_datatype_t dst_type,
  dart_handle_t*  handleptr)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    *handleptr = DART_HANDLE_NULL;
    dart_handle_t handle = *handleptr;

    size_t nbytes_elem = datatype_sizeof(datatype_base_struct(dts_src));
    size_t nbytes_segment = nbytes_elem * nelem;

    // initialized with relative team unit id
    dart_unit_t global_src_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_src_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_src_unit_id, &gaspi_src_seg_id, "dart_get_handled"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_src_unit_id)
    {
        DART_CHECK_ERROR(local_get(&gptr, gaspi_src_seg_id, dst, &conv_type));
        return DART_OK;
    }

    // get gaspi segment id and bind it to dst
    gaspi_segment_id_t free_seg_id;
    DART_CHECK_ERROR(seg_stack_pop(&dart_free_coll_seg_ids, &free_seg_id));

    gaspi_queue_id_t queue = (gaspi_queue_id_t) -1;

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_get(&gptr,
                   global_src_unit_id,
                   gaspi_src_seg_id,
                   free_seg_id,
                   dst,
                   &queue,
                   &conv_type)
    );

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        gaspi_notify(free_seg_id, global_my_unit_id.id, free_seg_id, free_seg_id, queue, GASPI_BLOCK));

    handle = (dart_handle_t) malloc(sizeof(struct dart_handle_struct));
    handle->comm_kind     = GASPI_READ;
    handle->queue         = queue;
    handle->local_seg_id  = free_seg_id;
    handle->notify_remote = 0;
    *handleptr = handle;

    free_converted_type(&conv_type);

    DART_LOG_DEBUG("dart_get_handle: handle(%p) dest:%d\n", (void*)(handle), global_src_unit_id);

    return DART_OK;

dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(free_seg_id));
    DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, free_seg_id));
    free_converted_type(&conv_type);

    return DART_ERR_OTHER;
}

dart_ret_t dart_put_handle(
  dart_gptr_t       gptr,
  const void      * src,
  size_t            nelem,
  dart_datatype_t   src_type,
  dart_datatype_t   dst_type,
  dart_handle_t   * handleptr)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    *handleptr = DART_HANDLE_NULL;
    dart_handle_t handle = *handleptr;

    // initialized with relative team unit id
    dart_unit_t global_dst_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_dst_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_dst_unit_id, &gaspi_dst_seg_id, "dart_put_handle"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_dst_unit_id)
    {
        DART_CHECK_ERROR(local_put(&gptr, gaspi_dst_seg_id, src, &conv_type));
        return DART_OK;
    }

    // get gaspi segment id and bind it to dst
    gaspi_segment_id_t free_seg_id;
    DART_CHECK_ERROR(seg_stack_pop(&dart_free_coll_seg_ids, &free_seg_id));

    gaspi_queue_id_t queue = (gaspi_queue_id_t) -1;

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_put(&gptr,
                   global_dst_unit_id,
                   gaspi_dst_seg_id,
                   free_seg_id,
                   src,
                   &queue,
                   &conv_type)
    );

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        gaspi_notify(free_seg_id, global_my_unit_id.id, free_seg_id, free_seg_id, queue, GASPI_BLOCK));

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label, put_completion_test(global_dst_unit_id, queue));

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        gaspi_notify(free_seg_id, global_my_unit_id.id, put_completion_dst_seg, put_completion_dst_seg, queue, GASPI_BLOCK));

    handle = (dart_handle_t) malloc(sizeof(struct dart_handle_struct));
    handle->comm_kind     = GASPI_WRITE;
    handle->queue         = queue;
    handle->local_seg_id  = free_seg_id;
    handle->notify_remote = put_completion_dst_seg;
    *handleptr = handle;

    free_converted_type(&conv_type);

    DART_LOG_DEBUG("dart_put_handle: handle(%p) dest:%d\n", (void*)(handle), global_dst_unit_id);

    return DART_OK;
dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(free_seg_id));
    DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, free_seg_id));
    free_converted_type(&conv_type);
}

dart_ret_t dart_flush(dart_gptr_t gptr)
{
    request_table_entry_t* request_entry = NULL;
    DART_CHECK_ERROR(find_rma_request(gptr.unitid, gptr.segid, &request_entry));
    if(request_entry == NULL)
    {
        DART_LOG_DEBUG("dart_flush: no queue found");

        return DART_OK;
    }

    DART_CHECK_GASPI_ERROR(gaspi_wait(request_entry->queue, GASPI_BLOCK));

    DART_CHECK_ERROR(free_segment_ids(request_entry));

    return DART_OK;
}

dart_ret_t dart_flush_all(dart_gptr_t gptr)
{
    request_table_entry_t* request_entry = NULL;

    request_iterator_t iter = new_request_iter(gptr.segid);
    if(request_iter_is_vaild(iter)){
      while(request_iter_is_vaild(iter))
      {
          DART_CHECK_ERROR(request_iter_get_entry(iter, &request_entry));

          DART_CHECK_ERROR(free_segment_ids(request_entry));

          DART_CHECK_ERROR(request_iter_next(iter));
      }

      DART_CHECK_ERROR(destroy_request_iter(iter));
   }
    return DART_OK;
}

dart_ret_t dart_flush_local(dart_gptr_t gptr)
{
    DART_CHECK_ERROR(dart_flush(gptr));

    return DART_OK;
}

dart_ret_t dart_flush_local_all(dart_gptr_t gptr)
{
    DART_CHECK_ERROR(dart_flush_all(gptr));

    return DART_OK;
}



dart_ret_t dart_get(
  void            * dst,
  dart_gptr_t       gptr,
  size_t            nelem,
  dart_datatype_t   src_type,
  dart_datatype_t   dst_type)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    size_t nbytes_elem = datatype_sizeof(datatype_base_struct(dts_src));
    size_t nbytes_segment = nbytes_elem * nelem;

    // initialized with relative team unit id
    dart_unit_t global_src_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_src_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_src_unit_id, &gaspi_src_seg_id, "dart_get_handled"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_src_unit_id)
    {
        DART_CHECK_ERROR(local_get(&gptr, gaspi_src_seg_id, dst, &conv_type));
        return DART_OK;
    }

    // get gaspi segment id and bind it to dst
    gaspi_segment_id_t free_seg_id;
    DART_CHECK_ERROR(seg_stack_pop(&dart_free_coll_seg_ids, &free_seg_id));

    // communitcation request
    request_table_entry_t* request_entry = NULL;
    DART_CHECK_ERROR_GOTO(dart_error_label,
        add_rma_request_entry(gptr.unitid, gptr.segid, free_seg_id, &request_entry));

    printf("[%d] set queue: %d - segid: %d\n", global_my_unit_id.id, request_entry->queue, request_entry->begin_seg_ids->local_gseg_id);
    fflush(stdout);
    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_get(&gptr,
                   global_src_unit_id,
                   gaspi_src_seg_id,
                   free_seg_id,
                   dst,
                   &(request_entry->queue),
                   &conv_type)
    );

    free_converted_type(&conv_type);

   return DART_OK;

dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(free_seg_id));
    DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, free_seg_id));
    //TODO remove segment id from request table
    free_converted_type(&conv_type);

    return DART_ERR_OTHER;
}

dart_ret_t dart_put(
  dart_gptr_t       gptr,
  const void      * src,
  size_t            nelem,
  dart_datatype_t   src_type,
  dart_datatype_t   dst_type)
{
    dart_datatype_struct_t* dts_src = get_datatype_struct(src_type);
    dart_datatype_struct_t* dts_dst = get_datatype_struct(dst_type);
    CHECK_EQUAL_BASETYPE(dts_src, dts_dst);

    // initialized with relative team unit id
    dart_unit_t global_dst_unit_id = gptr.unitid;

    gaspi_segment_id_t gaspi_dst_seg_id = 0;
    DART_CHECK_ERROR(glob_unit_gaspi_seg(&gptr, &global_dst_unit_id, &gaspi_dst_seg_id, "dart_put_handle"));

    dart_global_unit_t global_my_unit_id;
    DART_CHECK_ERROR(dart_myid(&global_my_unit_id));

    converted_type_t conv_type;
    DART_CHECK_ERROR(dart_convert_type(dts_src, dts_dst, nelem, &conv_type));

    if(global_my_unit_id.id == global_dst_unit_id)
    {
        DART_CHECK_ERROR(local_put(&gptr, gaspi_dst_seg_id, src, &conv_type));
        return DART_OK;
    }

    // get gaspi segment id and bind it to dst
    gaspi_segment_id_t free_seg_id;
    DART_CHECK_ERROR(seg_stack_pop(&dart_free_coll_seg_ids, &free_seg_id));

    // communitcation request
    request_table_entry_t* request_entry = NULL;
    DART_CHECK_ERROR_GOTO(dart_error_label,
        add_rma_request_entry(gptr.unitid, gptr.segid, free_seg_id, &request_entry));

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label,
        remote_put(&gptr,
                   global_dst_unit_id,
                   gaspi_dst_seg_id,
                   free_seg_id,
                   src,
                   &(request_entry->queue),
                   &conv_type)
    );

    DART_CHECK_GASPI_ERROR_GOTO(dart_error_label, put_completion_test(global_dst_unit_id, request_entry->queue));

    free_converted_type(&conv_type);

   return DART_OK;

dart_error_label:
    DART_CHECK_ERROR(gaspi_segment_delete(free_seg_id));
    DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, free_seg_id));
    //TODO remove segment id from request table
    free_converted_type(&conv_type);

    return DART_ERR_OTHER;
}


dart_ret_t dart_allreduce(
  const void       * sendbuf,
  void             * recvbuf,
  size_t             nelem,
  dart_datatype_t    dtype,
  dart_operation_t   op,
  dart_team_t        team)
{
  dart_team_unit_t        myid;
  size_t                  team_size;
  uint16_t                index;
  size_t                  elem_size = dart_gaspi_datatype_sizeof(dtype);
  DART_CHECK_ERROR(dart_team_myid(team, &myid));
  DART_CHECK_ERROR(dart_team_size(team, &team_size));

  if(dart_adapt_teamlist_convert(team, &index) == -1)
  {
    DART_LOG_ERROR(stderr, "dart_allreduce: can't find index of given team\n");
    return DART_ERR_OTHER;
  }
  /*
   * while DART supports 9 different datatypes when this was written
   * gaspi only supports 6. To keep DART compatible and easy to use
   * the gaspi_data_type isn't used in this case.
   */
  gaspi_reduce_state_t reduce_state = GASPI_STATE_HEALTHY;
  dart_ret_t ret = DART_OK;
  gaspi_group_t gaspi_group_id = dart_teams[index].id;
  switch (op) {
     case DART_OP_MINMAX:
     switch(dtype){
        case DART_TYPE_SHORT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_short,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_INT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_int,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_BYTE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_char,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_UINT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_uInt,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_long,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_ULONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_uLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONGLONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_longLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_FLOAT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_float,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_DOUBLE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MINMAX_double,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_MINMAX!!\n");
                 ret = DART_ERR_INVAL;
                 break;

     }
     break;
     case DART_OP_MIN:
     switch(dtype){
        case DART_TYPE_SHORT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_short,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_INT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_int,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_BYTE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_char,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_UINT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_uInt,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_long,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_ULONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_uLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONGLONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_longLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_FLOAT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_float,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_DOUBLE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_double,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_MIN!!\n");
                 ret = DART_ERR_INVAL;
                 break;
       }
       break;
       case DART_OP_MAX:
       switch(dtype){
          case DART_TYPE_SHORT:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_short,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_INT:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_int,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_BYTE:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_char,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_UINT:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_uInt,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_LONG:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_long,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_ULONG:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_uLong,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_LONGLONG:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_longLong,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_FLOAT:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_float,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          case DART_TYPE_DOUBLE:
            gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_double,
                                 reduce_state, gaspi_group_id, GASPI_BLOCK);
            break;
          default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_MAX!\n");
                   ret = DART_ERR_INVAL;
                   break;
       }
       break;
     case DART_OP_SUM:
     switch(dtype){
        case DART_TYPE_SHORT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_short,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_INT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_int,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_BYTE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_char,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_UINT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_uInt,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_long,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_ULONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_uLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_LONGLONG:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_longLong,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_FLOAT:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_float,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        case DART_TYPE_DOUBLE:
          gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_double,
                               reduce_state, gaspi_group_id, GASPI_BLOCK);
          break;
        default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_SUM!\n");
                 ret = DART_ERR_INVAL;
                 break;
     }
     break;
     case DART_OP_PROD:
       switch(dtype){
         case DART_TYPE_SHORT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_short,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_BYTE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_char,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_UINT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_uInt,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_LONG:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_long,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_ULONG:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_uLong,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_LONGLONG:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_longLong,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_FLOAT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_float,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_DOUBLE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_double,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
                ret = DART_ERR_INVAL;
                break;
     }

     break;
     case DART_OP_BAND:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BAND_char,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BAND_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LAND:
     switch(dtype){
      case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LAND_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
      default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_BOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BOR_char,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BOR_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LOR_char,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LOR_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_BXOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BXOR_char,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BXOR_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LXOR:
     switch(dtype){
      case DART_TYPE_INT:
         gaspi_allreduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LAND_int,
                              reduce_state, gaspi_group_id, GASPI_BLOCK);
         break;
      default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
               ret = DART_ERR_INVAL;
               break;
     }
     break;
     default: DART_LOG_ERROR(stderr, "dart_allreduce: operation not supported!\n" );
              ret = DART_ERR_INVAL;
  }
  return ret;
}

dart_ret_t dart_reduce(
  const void       * sendbuf,
  void             * recvbuf,
  size_t             nelem,
  dart_datatype_t    dtype,
  dart_operation_t   op,
  dart_team_unit_t   root,
  dart_team_t        team)
{
  dart_team_unit_t        myid;
  size_t                  team_size;
  gaspi_segment_id_t      gaspi_seg_id = dart_gaspi_buffer_id;
  gaspi_pointer_t         seg_ptr      = NULL;
  uint16_t                index;
  size_t                  elem_size = dart_gaspi_datatype_sizeof(dtype);
  DART_CHECK_ERROR(dart_team_myid(team, &myid));
  DART_CHECK_ERROR(dart_team_size(team, &team_size));

  if(dart_adapt_teamlist_convert(team, &index) == -1)
  {
    DART_LOG_ERROR(stderr, "dart_reduce: can't find index of given team\n");
    return DART_ERR_OTHER;
  }

  DART_CHECK_GASPI_ERROR(gaspi_segment_ptr(gaspi_seg_id, &seg_ptr));
  gaspi_segment_id_t* segment_ids = (gaspi_segment_id_t*) seg_ptr;

  for(size_t i = 0; i < team_size; i++)
  {
      segment_ids[i]=dart_fallback_seg;
  }

  /*
   * while DART supports 9 different datatypes when this was written
   * gaspi only supports 6. To keep DART compatible and easy to use
   * the gaspi_data_type isn't used in this case.
   */

  gaspi_reduce_state_t reduce_state = GASPI_STATE_HEALTHY;
  dart_ret_t ret = DART_OK;
  gaspi_group_t gaspi_group_id = dart_teams[index].id;
  dart_unit_t gaspi_root_proc;
  DART_CHECK_ERROR(unit_l2g(index, &gaspi_root_proc, root.id));

  switch (op) {
     case DART_OP_MIN:
     switch(dtype){
        case DART_TYPE_SHORT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_short,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_INT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_int,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_BYTE:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_char,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_UINT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_uInt,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_LONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_long,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_ULONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_uLong,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_LONGLONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_longLong,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_FLOAT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_float,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_DOUBLE:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_MIN_double,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_MIN!!\n");
                 ret = DART_ERR_INVAL;
                 break;
       }
       break;
     case DART_OP_MAX:
     switch(dtype){
          case DART_TYPE_SHORT:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_short,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_INT:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_int,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_BYTE:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_char,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_UINT:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_uInt,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_LONG:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_long,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_ULONG:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_uLong,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_LONGLONG:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_longLong,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_FLOAT:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_float,
                                 reduce_state, gaspi_group_id, segment_ids ,gaspi_root_proc, GASPI_BLOCK);
            break;
          case DART_TYPE_DOUBLE:
            gaspi_reduce_user(sendbuf, recvbuf, nelem,
                                 elem_size, gaspi_op_MAX_double,
                                 reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
            break;
          default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_MAX!\n");
                   ret = DART_ERR_INVAL;
                   break;
       }
       break;
     case DART_OP_SUM:
     switch(dtype){
        case DART_TYPE_SHORT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_short,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_INT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_int,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_BYTE:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_char,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_UINT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_uInt,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_LONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_long,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_ULONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_uLong,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_LONGLONG:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_longLong,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_FLOAT:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_float,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        case DART_TYPE_DOUBLE:
          gaspi_reduce_user(sendbuf, recvbuf, nelem,
                               elem_size, gaspi_op_SUM_double,
                               reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
          break;
        default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_SUM!\n");
                 ret = DART_ERR_INVAL;
                 break;
     }
     break;
     case DART_OP_PROD:
       switch(dtype){
         case DART_TYPE_SHORT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_short,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_BYTE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_char,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_UINT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_uInt,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_LONG:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_long,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_ULONG:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_uLong,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_LONGLONG:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_longLong,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_FLOAT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_float,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_DOUBLE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_PROD_double,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
                ret = DART_ERR_INVAL;
                break;
     }

     break;
     case DART_OP_BAND:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BAND_char,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BAND_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LAND:
     switch(dtype){
      case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LAND_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
      default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_BOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BOR_char,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BOR_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LOR_char,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LOR_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_BXOR:
     switch(dtype){
       case DART_TYPE_BYTE:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BXOR_char,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_BXOR_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
       default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_BAND!\n");
                ret = DART_ERR_INVAL;
                break;
     }
     break;
     case DART_OP_LXOR:
     switch(dtype){
      case DART_TYPE_INT:
         gaspi_reduce_user(sendbuf, recvbuf, nelem,
                              elem_size, gaspi_op_LAND_int,
                              reduce_state, gaspi_group_id, segment_ids, gaspi_root_proc, GASPI_BLOCK);
         break;
      default: DART_LOG_ERROR("ERROR: Datatype not supported for DART_OP_PROD!\n");
               ret = DART_ERR_INVAL;
               break;
     }
     break;
     default: DART_LOG_ERROR(stderr, "dart_reduce: operation not supported!\n" );
              ret = DART_ERR_INVAL;
  }
  //DART_CHECK_ERROR(seg_stack_push(&dart_free_coll_seg_ids, free_id));
  return ret;
}

dart_ret_t dart_recv(
  void                * recvbuf,
  size_t                nelem,
  dart_datatype_t       dtype,
  int                   tag,
  dart_global_unit_t    unit)
{
    DART_LOG_ERROR("dart_recv for gaspi not supported!");
    printf("dart_recv for gaspi not supported!\n");
    return DART_ERR_INVAL;
}

dart_ret_t dart_send(
  const void         * sendbuf,
  size_t               nelem,
  dart_datatype_t      dtype,
  int                  tag,
  dart_global_unit_t   unit)
{
    DART_LOG_ERROR("dart_send for gaspi not supported!");
    printf("dart_send for gaspi not supported!\n");
    return DART_ERR_INVAL;
}

dart_ret_t dart_sendrecv(
  const void         * sendbuf,
  size_t               send_nelem,
  dart_datatype_t      send_dtype,
  int                  send_tag,
  dart_global_unit_t   dest,
  void               * recvbuf,
  size_t               recv_nelem,
  dart_datatype_t      recv_dtype,
  int                  recv_tag,
  dart_global_unit_t   src)
{
    DART_LOG_ERROR("dart_sendrecv for gaspi not supported!");
    printf("dart_send for gaspi not supported!\n");
    return DART_ERR_INVAL;
}

//Needs ro be implemented
dart_ret_t dart_fetch_and_op(
  dart_gptr_t      gptr,
  const void *     value,
  void *           result,
  dart_datatype_t  dtype,
  dart_operation_t op)
{

  DART_LOG_ERROR("dart_fetch_and_op for gaspi not supported!");
  printf("dart_fetch_and_op for gaspi not supported!\n");
  return DART_ERR_INVAL;
}

//Needs to be implemented
dart_ret_t dart_accumulate(
  dart_gptr_t      gptr,
  const void *     value,
  size_t           nelem,
  dart_datatype_t  dtype,
  dart_operation_t op)
{
    printf("Entering dart_accumulate (gaspi)\n");

    void *     rec_value;
    auto teamid = gptr.teamid;
    auto segid = gptr.segid;

    dart_team_unit_t myrelid;
    dart_global_unit_t myid;
    dart_team_t myteamid;
    dart_myid(&myid);
    dart_team_myid(teamid, &myrelid);
    // custom reduction op`s ausschließen

    // check if dtype is basic type

    // convert dart_op to gaspi_op

    // get team id

    // get segment

    // get window?

    // prepare chunks -> need for maximum element value

    // here mpi calls mpi_accumulate
    dart_reduce(value, rec_value, nelem, dtype, op, myrelid, teamid);
    //

    return DART_OK;
}

dart_ret_t dart_accumulate_blocking_local(
    dart_gptr_t      gptr,
    const void     * values,
    size_t           nelem,
    dart_datatype_t  dtype,
    dart_operation_t op)
{
    DART_LOG_ERROR("dart_accumulate_blocking_local for gaspi not supported!");
    printf("dart_accumulate_blocking_local for gaspi not supported!\n");
    return DART_ERR_INVAL;
}

dart_ret_t dart_compare_and_swap(
    dart_gptr_t      gptr,
    const void     * value,
    const void     * compare,
    void           * result,
    dart_datatype_t  dtype)
{
    DART_LOG_ERROR("dart_compare_and_swap for gaspi not supported!");
    printf("dart_compare_and_swap for gaspi not supported!\n");
    return DART_ERR_INVAL;
}

dart_ret_t dart_alltoall(
    const void *    sendbuf,
    void *          recvbuf,
    size_t          nelem,
    dart_datatype_t dtype,
    dart_team_t     teamid)
{
    DART_LOG_ERROR("dart_alltoall for gaspi not supported!");
    printf("dart_alltoall for gaspi not supported!\n");
    return DART_ERR_INVAL;
}