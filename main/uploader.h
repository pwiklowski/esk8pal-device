#ifndef uploader_h
#define uplaoder_h


void uploader_init();
void uploader_sync_files();
void uploader_sync();

bool uploader_upload_file(char* filename, size_t size);

#endif