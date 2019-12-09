// TaskDownloader.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdio.h>
#include <string>
#include <curl/curl.h>
#include <openssl/md5.h>

#define WINDOWS
#ifdef WINDOWS
#include <direct.h>
#define GetCurrentDir _getcwd
#else
#include <unistd.h>
#define GetCurrentDir getcwd
#endif

#define BUFFER_SIZE 1024

#pragma comment (lib, "libcurl_a.lib")
#pragma comment (lib, "libssl.lib")
#pragma comment (lib, "libcrypto.lib")

using namespace std;


LPCSTR stringToLPCSTR(string text) {
	LPTSTR long_string = new TCHAR[text.size() + 1];
	strcpy(long_string, text.c_str());
	return long_string;
}

string GetCurrentWorkingDir() {
	char buff[FILENAME_MAX];
	GetCurrentDir(buff, FILENAME_MAX);
	string current_working_dir(buff);
	return current_working_dir;
}

unsigned long get_size_by_fd(int fd) {
	struct stat statbuf;
	if (fstat(fd, &statbuf) < 0) exit(-1);
	return statbuf.st_size;
}

size_t onWrite(void* ptr, size_t size, size_t nmemb, FILE* stream) {
	return fwrite(ptr, size, nmemb, stream);
}

bool onDownload(const char* url, const char outfilename[FILENAME_MAX]) {
	CURL* curl;
	CURLcode res;
	FILE* file;
	errno_t errFile;

	curl = curl_easy_init();

	errFile = fopen_s(&file, outfilename, "wb");

	if (errFile == 0) {
		if (curl) {
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onWrite);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

			res = curl_easy_perform(curl);

			if (res == CURLE_OK) {		
				long response_code;
				curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

				if (response_code == 200) {
					fclose(file);
					curl_easy_cleanup(curl);
					printf("*** The file has been downloaded. \n");
					return true;
				} else {
					fclose(file);
					curl_easy_cleanup(curl);
					printf("*** HTTP status is unsatisfactory. \n");
					return false;
				}
			} else {
				fclose(file);
				curl_easy_cleanup(curl);
				printf("*** The file has not been downloaded. \n");
				return false;
			}
		} else {
			fclose(file);
			curl_easy_cleanup(curl);
			printf("*** We have problems with CURL. \n");
			return false;
		}
	} else {
		printf("*** Can't open file for writing downloaded file. \n");
		return false;
	}
}

bool onLoadFile(const char* u_name, const char* f_name) {
	FILE* file;
	char line[BUFFER_SIZE];
	errno_t errFile;

	errFile = fopen_s(&file, "mirrors.list", "r");

	if (errFile == 0) {
		while (fgets(line, BUFFER_SIZE, file)) {
			if (!feof(file))
				line[strlen(line) - 1] = '\0';

			strncat_s(line, u_name, BUFFER_SIZE - strlen(line) - 1);
			
			printf("*** Try to download from %s \n", line);
			
			if (onDownload(line, f_name)) {
				fclose(file);
				return true;
			}
		}

		fclose(file);

		return false;
	} else {
		printf("*** The file 'mirrors.list' doesn't open.\n");
		return false;
	}
}

void onUpdateMirrors() {
	const char* u_name = "mirrors.list";

	if (onLoadFile(u_name, "mirrors.tmp")) {
		remove(u_name);
		rename("mirrors.tmp", "mirrors.list");		
		printf("*** Mirrors updated.\n");
	}
}

char* onLoadTask(bool &isRunning) {
	const char* task_file = "task.file";
		
	if (onLoadFile(task_file, task_file)) {
		FILE* file;
		char line[BUFFER_SIZE];
		errno_t errFile;

		errFile = fopen_s(&file, task_file, "r");

		if (errFile == 0) {
			int l_num = 0;
			char md5sum[BUFFER_SIZE];

			printf("*** Download task:\n");

			while (fgets(line, BUFFER_SIZE, file)) {
				if (!feof(file))
					line[strlen(line) - 1] = '\0';
				
				switch (l_num) {
					case 0:
						if (strcmp(line, "run") == 0)
							isRunning = true;
						else
							isRunning = false;
						
						printf("Action: %s\n", line);

						break;
					case 1:
						printf("md5: %s \n", line);
						strcpy_s(md5sum, sizeof md5sum, line);
						break;
					case 2:
						printf("file: %s \n", line);

						if (onLoadFile(line, line)) {
							unsigned char c[MD5_DIGEST_LENGTH];
							int i;
							FILE* dlFile;
							errno_t errDlFile;
							errDlFile = fopen_s(&dlFile, line, "rb");
							MD5_CTX mdContext;
							int bytes;
							unsigned char data[BUFFER_SIZE];
							char fullhex[BUFFER_SIZE] = { '\0' };

							if (errDlFile == 0) {
								MD5_Init(&mdContext);

								while ((bytes = fread(data, 1, BUFFER_SIZE, dlFile)) != 0)
									MD5_Update(&mdContext, data, bytes);

								MD5_Final(c, &mdContext);

								for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
									char hex[64];
									sprintf_s(hex, sizeof hex, "%02x", c[i]);
									strncat_s(fullhex, hex, BUFFER_SIZE - strlen(fullhex) - 1);
								}

								fclose(dlFile);

								printf("*** md5: %.*s\n", (int) sizeof(fullhex), fullhex);

								for (unsigned int a = 0; a < strlen(md5sum) - 1; a++) {
									if (fullhex[a] != md5sum[a]) {
										printf("*** MD5 sum did not match. \n");
										return NULL;
									}
								}

								printf("*** MD5 sum matched. \n");

								return line;
							}
							else {
								printf("*** %s can't be opened.\n", line);
								return NULL;
							}
						}

						break;
					default:
						break;
				}

				l_num++;
			}
			fclose(file);
		} else {
			printf("*** Task file doesn't open. \n");
			return NULL;
		}		
	}
}

int main() {
	ShellExecute(0, stringToLPCSTR("open"), stringToLPCSTR("ipconfig /flushdns"), NULL, 0, SW_HIDE);

	onUpdateMirrors();
	
	bool isRunning = false;
	char* task = onLoadTask(isRunning);

	if (task != NULL && isRunning) {
		char tmpTask[BUFFER_SIZE];
		strcpy_s(tmpTask, sizeof tmpTask, task);

		
		string path = GetCurrentWorkingDir();
		path.append("\\");
		path.append(tmpTask);

		printf("*** Running %.*s", (int) sizeof(tmpTask), tmpTask);

		ShellExecute(0, stringToLPCSTR("open"), stringToLPCSTR(path), NULL, 0, SW_HIDE);		
	}
	
	return 0;    
}