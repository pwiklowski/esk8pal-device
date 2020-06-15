#ifndef uploader_h
#define uplaoder_h


void uploader_init();
void uploader_sync_files();
void uploader_sync();
uint16_t uploader_count_files_to_be_uploaded();
bool uploader_is_task_running();
bool uploader_upload_file(char* filename, size_t size);

#endif