#include "entity_data.h"

static void delete(entity_data_t*);


entity_data_t* newEntityData(int32_t fd, void* context,
	delete_context_fptr delete_context_hdlr,
	handle_event_fptr read_hdlr, handle_event_fptr write_hdlr,
	handle_event_fptr error_hdlr, handle_event_fptr hup_hdlr,
	handle_event_fptr timeout_hdlr) {

    entity_data_t* ent_data =
	(entity_data_t*)calloc(1, sizeof(entity_data_t));
    if (ent_data) {
	ent_data->fd = fd;
	ent_data->scheduled.tv_sec = 0;
	ent_data->scheduled.tv_usec = 0;
	ent_data->to_be_scheduled.tv_sec = 0;
	ent_data->to_be_scheduled.tv_usec = 0;
	ent_data->event_time.tv_sec = 0;
	ent_data->event_time.tv_usec = 0;
	ent_data->event_flags = 0;

	ent_data->context = context;
	ent_data->read_hdlr = read_hdlr;
	ent_data->write_hdlr = write_hdlr;
	ent_data->error_hdlr = error_hdlr;
	ent_data->hup_hdlr = hup_hdlr;
	ent_data->timeout_hdlr = timeout_hdlr;
	ent_data->delete_context_hdlr = delete_context_hdlr;
	ent_data->delete_hdlr = delete;
    }
    return ent_data;

}


static void delete(entity_data_t* ent_data) {

    if ((ent_data->delete_context_hdlr) && (ent_data->context))
	ent_data->delete_context_hdlr(ent_data->context);
    free(ent_data);
}

