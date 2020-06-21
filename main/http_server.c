#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_http_server.h"
#include "esp_spiffs.h"
#include "esp_vfs.h"

const char *HTTP_TAG = "HttpServer";

/* Scratch buffer size */
#define SCRATCH_BUFSIZE 8192

struct file_server_data {
  /* Base path of file storage */
  char base_path[ESP_VFS_PATH_MAX + 1];

  /* Scratch buffer for temporary storage during file transfer */
  char scratch[SCRATCH_BUFSIZE];
};

struct file_server_data server_data;
#define FILE_PATH_MAX 50

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char *get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize) {
  const size_t base_pathlen = strlen(base_path);
  size_t pathlen = strlen(uri);

  const char *quest = strchr(uri, '?');
  if (quest) {
    pathlen = MIN(pathlen, quest - uri);
  }
  const char *hash = strchr(uri, '#');
  if (hash) {
    pathlen = MIN(pathlen, hash - uri);
  }

  if (base_pathlen + pathlen + 1 > destsize) {
    /* Full path string won't fit into destination buffer */
    return NULL;
  }

  /* Construct full path (base + path) */
  strcpy(dest, base_path);
  strlcpy(dest + base_pathlen, uri, pathlen + 1);

  /* Return pointer to path, skipping the base */
  return dest + base_pathlen;
}

static esp_err_t http_resp_dir_html(httpd_req_t *req, const char *dirpath) {
  char entrypath[FILE_PATH_MAX];
  char entrysize[16];
  const char *entrytype;

  struct dirent *entry;
  struct stat entry_stat;

  DIR *dir = opendir(dirpath);
  const size_t dirpath_len = strlen(dirpath);

  /* Retrieve the base path of file storage to construct the full path */
  strlcpy(entrypath, dirpath, sizeof(entrypath));

  if (!dir) {
    ESP_LOGE(HTTP_TAG, "Failed to stat dir : %s", dirpath);
    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Directory does not exist");
    return ESP_FAIL;
  }

  /* Send HTML file header */
  httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");

  /* Send file-list table definition and column labels */
  httpd_resp_sendstr_chunk(
      req, "<table class=\"fixed\" border=\"1\">"
           "<col width=\"800px\" /><col width=\"300px\" /><col width=\"300px\" /><col width=\"100px\" />"
           "<thead><tr><th>Name</th><th>Type</th><th>Size (Bytes)</th><th>Delete</th></tr></thead>"
           "<tbody>");

  /* Iterate over all files / folders and fetch their names and sizes */
  while ((entry = readdir(dir)) != NULL) {
    entrytype = (entry->d_type == DT_DIR ? "directory" : "file");

    strlcpy(entrypath + dirpath_len, entry->d_name, sizeof(entrypath) - dirpath_len);
    if (stat(entrypath, &entry_stat) == -1) {
      ESP_LOGE(HTTP_TAG, "Failed to stat %s : %s", entrytype, entry->d_name);
      continue;
    }
    sprintf(entrysize, "%ld", entry_stat.st_size);
    ESP_LOGI(HTTP_TAG, "Found %s : %s (%s bytes)", entrytype, entry->d_name, entrysize);

    /* Send chunk of HTML file containing table entries with file name and size */
    httpd_resp_sendstr_chunk(req, "<tr><td><a href=\"");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    if (entry->d_type == DT_DIR) {
      httpd_resp_sendstr_chunk(req, "/");
    }
    httpd_resp_sendstr_chunk(req, "\">");
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "</a></td><td>");
    httpd_resp_sendstr_chunk(req, entrytype);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, entrysize);
    httpd_resp_sendstr_chunk(req, "</td><td>");
    httpd_resp_sendstr_chunk(req, "<form method=\"post\" action=\"/delete");
    httpd_resp_sendstr_chunk(req, req->uri);
    httpd_resp_sendstr_chunk(req, entry->d_name);
    httpd_resp_sendstr_chunk(req, "\"><button type=\"submit\">Delete</button></form>");
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");
  }
  closedir(dir);

  /* Finish the file list table */
  httpd_resp_sendstr_chunk(req, "</tbody></table>");

  /* Send remaining chunk of HTML file to complete it */
  httpd_resp_sendstr_chunk(req, "</body></html>");

  /* Send empty chunk to signal HTTP response completion */
  httpd_resp_sendstr_chunk(req, NULL);
  return ESP_OK;
}

/* Handler to redirect incoming GET request for /index.html to /
 * This can be overridden by uploading file with same name */
static esp_err_t index_html_get_handler(httpd_req_t *req) {
  httpd_resp_set_status(req, "307 Temporary Redirect");
  httpd_resp_set_hdr(req, "Location", "/");
  httpd_resp_send(req, NULL, 0); // Response body can be empty
  return ESP_OK;
}

static esp_err_t download_get_handler(httpd_req_t *req) {
  char filepath[FILE_PATH_MAX];
  FILE *fd = NULL;
  struct stat file_stat;

  const char *filename =
      get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path, req->uri, sizeof(filepath));

  ESP_LOGE(HTTP_TAG, "get %s", filename);
  if (!filename) {
    ESP_LOGE(HTTP_TAG, "Filename is too long");
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Filename too long");
    return ESP_FAIL;
  }

  /* If name has trailing '/', respond with directory contents */
  if (filename[strlen(filename) - 1] == '/') {
    return http_resp_dir_html(req, filepath);
  }

  if (stat(filepath, &file_stat) == -1) {
    /* If file not present on SPIFFS check if URI
     * corresponds to one of the hardcoded paths */
    if (strcmp(filename, "/index.html") == 0) {
      return index_html_get_handler(req);
    }
    ESP_LOGE(HTTP_TAG, "Failed to stat file : %s", filepath);
    /* Respond with 404 Not Found */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File does not exist");
    return ESP_FAIL;
  }

  fd = fopen(filepath, "r");
  if (!fd) {
    ESP_LOGE(HTTP_TAG, "Failed to read existing file : %s", filepath);
    /* Respond with 500 Internal Server Error */
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read existing file");
    return ESP_FAIL;
  }

  ESP_LOGI(HTTP_TAG, "Sending file : %s (%ld bytes)...", filename, file_stat.st_size);
  httpd_resp_set_type(req, "text/plain");

  /* Retrieve the pointer to scratch buffer for temporary storage */
  char *chunk = ((struct file_server_data *)req->user_ctx)->scratch;
  size_t chunksize;
  do {
    /* Read file in chunks into the scratch buffer */
    chunksize = fread(chunk, 1, SCRATCH_BUFSIZE, fd);

    /* Send the buffer contents as HTTP response chunk */
    if (httpd_resp_send_chunk(req, chunk, chunksize) != ESP_OK) {
      fclose(fd);
      ESP_LOGE(HTTP_TAG, "File sending failed!");
      /* Abort sending file */
      httpd_resp_sendstr_chunk(req, NULL);
      /* Respond with 500 Internal Server Error */
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
      return ESP_FAIL;
    }

    /* Keep looping till the whole file is sent */
  } while (chunksize != 0);

  /* Close file after sending complete */
  fclose(fd);
  ESP_LOGI(HTTP_TAG, "File sending complete");

  /* Respond with an empty chunk to signal HTTP response completion */
  httpd_resp_send_chunk(req, NULL, 0);
  return ESP_OK;
}

esp_err_t start_file_server(const char *base_path) {

  strlcpy(server_data.base_path, base_path, sizeof(server_data.base_path));

  httpd_handle_t server = NULL;
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();

  config.uri_match_fn = httpd_uri_match_wildcard;

  ESP_LOGI(HTTP_TAG, "Starting HTTP Server");
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(HTTP_TAG, "Failed to start file server!");
    return ESP_FAIL;
  }

  httpd_uri_t file_download = {
      .uri = "/*", // Match all URIs of type /path/to/file
      .method = HTTP_GET,
      .handler = download_get_handler,
      .user_ctx = &server_data // Pass server data as context
  };
  httpd_register_uri_handler(server, &file_download);

  return ESP_OK;
}