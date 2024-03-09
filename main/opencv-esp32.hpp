#undef EPS
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

#include <string>
#include "sdkconfig.h"
#include <iostream>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <esp_err.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <microhttpd/microhttpd.h>
#include <esp_camera.h>
#include <sdmmc_cmd.h>
#include "driver/sdmmc_host.h"
#include <esp_vfs_fat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace cv;
using namespace std;

//network consts
#define SSID = "HUAWEI-2.4G-9e9d"
#define PASSWORD = "acce69e1"



//define pins
// #define CLK_PIN 14
// #define CMD_PIN 15
// #define DATA0_PIN 2
// #define DATA1_PIN 4     //DATA1 is the same pin as flashlight
// #define FLASHLIGHT_PIN 4
// #define DATA2_PIN 12
// #define DATA3_PIN 13

//define shape type
enum shape_t {triangle_s, circle_s, unknown_s};

//init functions
void setup(void);
bool init_dns(void);
bool init_network(void);
bool init_server(void);
bool init_camera(void);
bool init_storage(void);
void critical_failure(void);
void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data);
//storage functions
string getFileNameBmp(string post);
string getFileNamePng(string post);

//numbering functions
uint16_t getPictureNumber(void);
void incPictureNumber(void);
void decPictureNumber(void);
void initPictureNumbering(void);
//camera functions
void takePhotoPng(bool shouldIncrement);
void takePhotoBmp(bool shouldIncrement);
void takePhotoToMat(Mat& result);
char* generateBmpHeader(u_int32_t map_size);

//detect shape function
shape_t parseImage(cv::String filepath);
shape_t parseImageFromMat(cv::Mat &img);

//server functions
static void send_not_found(tMicroHttpdClient client, const char *uri);
static void handle_root(tMicroHttpdClient client, const char *uri,
const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);
static void handle_photo(tMicroHttpdClient client, const char *uri,
const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie);

//helpers
uint32_t stringToUint32(string input);
uint16_t stringToUint16(string input);
uint8_t stringToUint8(string input);
string Uint32ToString(uint32_t input);
string Uint16ToString(uint16_t input);
string Uint8ToString(uint8_t input);
string writeShapeName(shape_t input);
string IPtoString(uint8_t *ip);
void TaskHttpServer(void * parameters);


extern "C" {
    void app_main(void);
}

static tMicroHttpdGetHandlerEntry get_handler_list[] =
{
   { "/photo", handle_photo, NULL },
   { "/", handle_root, NULL}
};