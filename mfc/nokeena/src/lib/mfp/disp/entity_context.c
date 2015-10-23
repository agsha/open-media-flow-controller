#include "entity_context.h"

static void deleteEntityContext(entity_context_t*);


entity_context_t* newEntityContext(int32_t fd,
        void* context_data, freeContextData free_data_handler,
        handleEvent read_handler, handleEvent write_handler,
        handleEvent error_handler, handleEvent hup_handler,
        handleEvent timeout_handler, struct dispatcher_mngr* disp_mngr) {

        entity_context_t* ent_context =
                (entity_context_t*)disp_calloc(1, sizeof(entity_context_t));
        if (ent_context) {
                ent_context->fd = fd;
                ent_context->disp_mngr = disp_mngr;
                ent_context->scheduled.tv_sec = 0;
                ent_context->scheduled.tv_usec = 0;
                ent_context->to_be_scheduled.tv_sec = 0;
                ent_context->to_be_scheduled.tv_usec = 0;
                ent_context->event_time.tv_sec = 0;
                ent_context->event_time.tv_usec = 0;
		ent_context->event_flags = 0;

                ent_context->context_data = context_data;
                ent_context->event_read = read_handler;
                ent_context->event_write = write_handler;
                ent_context->event_error = error_handler;
                ent_context->event_hup = hup_handler;
                ent_context->event_hup = hup_handler;
                ent_context->event_timeout = timeout_handler;
                ent_context->free_data_handler = free_data_handler;
		ent_context->delete_entity = deleteEntityContext;

        }
        return ent_context;

}


static void deleteEntityContext(entity_context_t* ent_context) {

        if ((ent_context->free_data_handler) && (ent_context->context_data))
                ent_context->free_data_handler(ent_context->context_data);
        free(ent_context);
}
