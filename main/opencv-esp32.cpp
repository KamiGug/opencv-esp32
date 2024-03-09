#undef EPS
#include "opencv2/opencv.hpp"
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/imgcodecs.hpp"
#define EPS 192

#include "opencv-esp32.hpp"
#include <string>
#include "sdkconfig.h"
#include <iostream>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <unistd.h>
#include <esp_err.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <mdns.h>
#include "nvs_flash.h"
#include <microhttpd/microhttpd.h>
#include <sdmmc_cmd.h>
#include "driver/sdmmc_host.h"
#include <esp_vfs_fat.h>
#include <libpng/png.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

//Camera defines

#define BOARD_ESP32CAM_AITHINKER
#include <esp_camera.h>
#ifdef BOARD_ESP32CAM_AITHINKER
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1 //software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27
#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22
#endif
#if ESP_CAMERA_SUPPORTED
static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,

    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,

    //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_GRAYSCALE, //YUV422,GRAYSCALE,RGB565,JPEG
    .frame_size = FRAMESIZE_SVGA,    //QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The performance of the ESP32-S series has improved a lot, but JPEG mode always gives better frame rates.

    .jpeg_quality = 12, //0-63, for OV series camera sensors, lower number means higher quality
    .fb_count = 1,       //When jpeg mode is used, if fb_count more than one, the driver will work in continuous mode.
    .grab_mode = CAMERA_GRAB_LATEST,
};
#define WIDTH 800
#define HEIGHT 600
#endif

//internet connection consts
#define WIFI_SSID "Here"
#define WIFI_PASS "23456789"

uint8_t ip_adress[4];
#define SERVER_PORT 80

static tMicroHttpdContext ctx;

using namespace cv;
using std::cout;
using std::cin;
using std::endl;
using std::string;



//storage consts / variables
#define MOUNT_POINT "/SD"

//numbering variables
#define MAX_SAVED_IMAGES 5
uint16_t photoNumber = 0; //variable holding number of the last taken photo 

//time variables
uint32_t delayForPhotos; //keep the delay between each photo
uint32_t timestamp; //needed for delay to work well

static int debug = 0;

void app_main(void)
{
    setup();
    TaskHandle_t _serverHandle = NULL;        //prepare a handle for managing server requests
    cout << "before add http" << endl;
    xTaskCreate(
        TaskHttpServer,
        "server",
        8000,
        NULL,
        1,
        &_serverHandle
        );
    

    while (true) {
        takePhotoPng(true);
        cout << writeShapeName(parseImage(getFileNamePng(Uint16ToString(getPictureNumber()).c_str()))) << endl << std::flush;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}


//setup functions

void setup() {
    nvs_flash_init();
    if (init_camera()) {
        cout << "Camera OK!" << endl;
    }
    else {
        cout << "error: Unable to init camera!" << endl;
        critical_failure();
    }
    if (init_storage()) {
        cout << "Storage OK!" << endl;
    } 
    else {
        cout << "error: Unable to init storage!" << endl;
        critical_failure();
    }
    if (init_network()) {
        cout << "Wifi OK!" << endl;
        if (init_dns()) {
            cout << "DNS OK!" << endl;
        }
        else {
            cout << "DNS error!" << endl;
        }
        if (init_server()) {
            cout << "Server is up!"<< endl;
            //TODO: print out IP
            //TODO: print out the url
        } 
        else {
            cout << "error: Server is NOT up!"<< endl;
        }
    } 
    else {
        cout << "Wifi error!"<< endl;
    }
    // cout << getBuildInformation();
}

bool init_dns() {
    char *hostname = "esp32";

    //initialize mDNS
    if (mdns_init() != ESP_OK) return false;
    if (mdns_hostname_set(hostname) != ESP_OK) return false;
    cout << "mdns hostname set to: " << hostname << endl;
    //set default mDNS instance name
    if (mdns_instance_name_set("ESP32-WebServer") != ESP_OK) return false;

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}
    };

    //initialize service
    ESP_ERROR_CHECK( mdns_service_add("ESP32-WebServer", "_http", "_tcp", SERVER_PORT, serviceTxtData, 3) );
    ESP_ERROR_CHECK( mdns_service_subtype_add_for_host("ESP32-WebServer", "_http", "_tcp", NULL, "_server") );

    // //add another TXT item
    // ESP_ERROR_CHECK( mdns_service_txt_item_set("_http", "_tcp", "path", "/foobar") );
    // //change TXT item value
    // ESP_ERROR_CHECK( mdns_service_txt_item_set_with_explicit_value_len("_http", "_tcp", "u", "admin", strlen("admin")) );
    // free(hostname);

    return true;
}

void wifi_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,void *event_data) {
    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        cout << "WIFI CONNECTING...." << endl;
        break;
    
    case WIFI_EVENT_STA_CONNECTED:
        cout << "WiFi CONNECTED" << endl;
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        cout << "WiFi lost connection" << endl;
        esp_wifi_connect();
        break;
    case IP_EVENT_STA_GOT_IP:
        cout << "Wifi got IP..." << endl << endl;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ip_adress[0] = event->ip_info.ip.addr;
        ip_adress[1] = event->ip_info.ip.addr >> 8;
        ip_adress[2] = event->ip_info.ip.addr >> 16;
        ip_adress[3] = event->ip_info.ip.addr >> 24;
        

        cout <<  IPtoString(ip_adress) << endl;
        break;
    }
}

bool init_network() {
    if (esp_netif_init() != ESP_OK) return false;                       //init networking
    if (esp_event_loop_create_default() != ESP_OK) return false;        //init event loop
    esp_netif_create_default_wifi_sta();                                //create WiFi STA (station/client)
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();    //get default wifi config
    esp_wifi_init(&wifi_initiation);                                    //init WiFi STA (station/client)
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
           }
        };
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_configuration) != ESP_OK) return false;
    if (esp_wifi_start() != ESP_OK) return false;
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) return false;
    while (esp_wifi_connect() != ESP_OK);
    while (ip_adress[0] == 0 && ip_adress[1] == 0 && ip_adress[2] == 0 && ip_adress[3] == 0){
        vTaskDelay( 500 );
    }
    
    return true;
}

bool init_server() {
    tMicroHttpdParams params = {0};

   params.server_port = SERVER_PORT;
   params.process_timeout = 0;
   params.rx_buffer_size = 2048;
//    params.post_handler = post_handler;
   params.get_handler_list = get_handler_list;
   params.get_handler_count = (sizeof(get_handler_list)/sizeof((get_handler_list)[0]));
//    params.default_get_handler = handle_file;

   ctx = microhttpd_start(&params);
   if(NULL == ctx)
   {
      cout << "Failed to initialize microhttpd" << endl;
      return false;
   }

//    DBG("Server started\n");
//    while(microhttpd_process(ctx) == 0);
//    DBG("Server terminated\n");

    return true;
}



bool init_camera() {
    #if ESP_CAMERA_SUPPORTED
        if (esp_camera_init(&camera_config) == ESP_OK) {
            return true;
        }
    #endif
    return false;
}

void critical_failure() {
    cout << "CRITICAL FAILURE!" << endl << std::flush;
    esp_restart();
}

bool init_storage() {
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 5 * 1024,
        .disk_status_check_enable = true
        };

    sdmmc_card_t *card;

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.width = 1;
    
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    FILE *f;
    string test_filename = MOUNT_POINT;
    test_filename.append("/test.txt");
    if (esp_vfs_fat_sdmmc_mount(MOUNT_POINT, &host, &slot_config, &mount_config, &card) == ESP_OK && (f = fopen(test_filename.c_str(), "w"))) {
        fclose(f);
        initPictureNumbering();
        return true;
    } 
    else {
        return false;
    }
}

//storage functions - getting paths, numbers, etc.
string getFileNameBmp(string post) {
    string tmp = "";
    tmp.append(MOUNT_POINT);
    tmp.append("/photo");
    tmp.append(post);
    tmp.append(".bmp");
    return tmp;
}

string getFileNamePng(string post) {
    string tmp = "";
    tmp.append(MOUNT_POINT);
    tmp.append("/photo");
    tmp.append(post);
    tmp.append(".png");
    return tmp;
}

//helpers
string writeShapeName(shape_t input) {
    switch (input) {
        case shape_t::triangle_s:
            return "triangle";
        case shape_t::circle_s:
            return "circle";
        case shape_t::unknown_s:
            return "unknown";
    }
    return "";
}

uint32_t stringToUint32 (string input) { 
    uint32_t tmp = 0;
    for (uint8_t i = 0; i < input.length(); i++) {
        tmp = tmp * 10 + (input[i] - '0');
    }
    return tmp;
}

uint16_t stringToUint16 (string input) { 
    uint16_t tmp = 0;
    for (uint8_t i = 0; i < input.length(); i++) {
        tmp = tmp * 10 + (input[i] - '0');
    }
    return tmp;
}

uint8_t stringToUint8 (string input) {
    uint8_t tmp = 0;
    for (uint8_t i = 0; i < input.length(); i++) {
        tmp = tmp * 10 + (input[i] - '0');
    }
    return tmp;
}

string Uint32ToString (uint32_t input) {
    string tmp = "";
    do {
        tmp.insert(0, 1, (input % 10) + '0');
        input /= 10;
    } while (input != 0);
    return tmp;
}

string Uint16ToString (uint16_t input) {
    string tmp = "";
    do {
        tmp.insert(0, 1, (input % 10) + '0');
        input /= 10;
    } while (input != 0);
    return tmp;
}

string Uint8ToString (uint8_t input) {
    string tmp = "";
    do {
        tmp.insert(0, 1, (input % 10) + '0');
        input /= 10;
    } while (input != 0);
    return tmp;
}

string IPtoString(uint8_t *ip) {
    return Uint8ToString(ip_adress[0]) + "." + Uint8ToString(ip_adress[1]) + "." 
    + Uint8ToString(ip_adress[2]) + "." + Uint8ToString(ip_adress[3]);
}

void TaskHttpServer(void * pvParameters) {
    for (;;) {
        microhttpd_process(ctx);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

//numbering functions
uint16_t getPictureNumber() {
  return photoNumber;
}

void incPictureNumber() {
    photoNumber = (photoNumber + 1) % MAX_SAVED_IMAGES;
}

void decPictureNumber() {
    photoNumber = (photoNumber + MAX_SAVED_IMAGES - 1) % MAX_SAVED_IMAGES;
}

void initPictureNumbering() {
    photoNumber = 0;
    // int16_t i = 0;
    // struct stat buffer;
    // while (stat(getFileName(Uint16ToString(i)).c_str(), &buffer) == 0) i++;
    // if (i > 0) {
    //     photoNumber = i-1;
    // }
    // else {
    //     photoNumber = 0;
    // }
}

//camera functions
void takePhotoPng(bool shouldIncrement) {
    camera_fb_t *fb = NULL;
    if (shouldIncrement) incPictureNumber();
    string name = getFileNamePng(Uint8ToString(getPictureNumber()));
    FILE *f;
    // uint8_t * buf = NULL;
    // size_t buf_len = 0;
    fb = esp_camera_fb_get();  //uruchomienie kamery 
    if (!fb) {
        cout << "Camera capture failed" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }
    f = fopen(name.c_str(), "wb");
    if (f == NULL) {
        cout << "Failed to open file in writing mode" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }
    if (fb->len != WIDTH * HEIGHT) {
        cout << "Image is corrupted!" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }

    cout << "writing at " << name << endl;

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (png == NULL) {
        cout << "Failed to create png write struct" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }
    png_infop info = png_create_info_struct(png);
    if (info == NULL) {
        cout << "Failed to create png info struct" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }
    if (setjmp(png_jmpbuf(png))) {
        cout << "Set return jump failure" << std::flush;
        if (shouldIncrement) decPictureNumber();
        return;
    }
    png_init_io(png, f);
    png_set_IHDR(
        png,
        info,
        WIDTH, HEIGHT,
        8,
        PNG_COLOR_TYPE_GRAY,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    // cout << "fb->len = " << fb->len << endl << std::flush;
    //TODO: check if this works
    uint8_t **row_pointers = (uint8_t**) malloc (sizeof(uint8_t*) * HEIGHT);
    for (uint16_t i = 0; i < HEIGHT; i++) {
        row_pointers[i] = fb->buf + WIDTH * i;
    }
    //make a 2d array using fb->buf

    // for (uint16_t i = 0; i < HEIGHT; i++) {
    png_write_image(png, row_pointers);
    // }

    // png_write_image(png, row_pointers);
    // png_write_end(png, NULL);

    esp_camera_fb_return(fb);
    // free(fb->buf);
    // fclose(f);
    
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    // for(uint16_t y = 0; y < HEIGHT; y++) {
    //     cout << "free y = " << Uint16ToString(y) << endl;
    //     free(row_pointers[y]);
    // }
    free(row_pointers);

    // fwrite(fb->buf, sizeof(uint8_t), fb->len, f);
    fclose(f);
}

//server functions
static void send_not_found(tMicroHttpdClient client, const char *uri)
{
   int urilen = strlen(uri);
   char *content;

   if(urilen > 50)
      urilen = 50;
   urilen += 60;
   content = (char*) malloc(urilen);
   if(NULL == content)
      return;

   snprintf(content, urilen, "<html><title>Not Found</title>Not found: %s</html>", uri);
   microhttpd_send_response(client, HTTP_NOT_FOUND, "text/html", strlen(content), NULL, content);
   free(content);
}

static void handle_root(tMicroHttpdClient client, const char *uri,
const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie) {
    char content[] = "<html>Hello there!</html>";
    cout << "at handle root" << endl;
    microhttpd_send_response(client, HTTP_OK, "text/html", strlen(content), NULL, content); 


}

static void handle_photo(tMicroHttpdClient client, const char *uri,
const char *param_list[], const uint32_t param_count, const char *source_address, void *cookie) {
    // if (param_count < 1)
    // {
    //     /* code */
    // }
    char content[] = "asd";
    microhttpd_send_response(client, HTTP_OK, "image/png", strlen(content), NULL, content); 
}




//shape detect function
shape_t parseImage(cv::String filepath) {
    Mat img = imread(filepath, IMREAD_GRAYSCALE);
    // Mat imgT;
    // threshold(img, imgT, 100, 150, THRESH_BINARY);
    Mat img1;
    blur(img, img1, Size(3, 3));
    threshold(img1, img, 100, 150, THRESH_BINARY);
    // Canny(img, img1, 100, 300, 3);
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    findContours(img, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
    if (contours.size() <= 0) {
        return shape_t::unknown_s;
    }
    vector<Point> conPoly;                      //polygon that detected in a shape
    RotatedRect ellipse;                        //elipse to check if object is round enough to be a circle
    double axis[2] = {0};
    double eccentricity, tmp_area, tmp_area_ellipse, biggest_area = 0, perimeter;
    shape_t biggest_result = unknown_s;
    Mat points_for_ellipse;
    Point2f ellipse_def_points[4];
    for (int i = 0; i < contours.size(); i++) {
        tmp_area = contourArea(contours[i]);
        if(tmp_area > 200 && biggest_area < contourArea(contours[i])) {    //filter out noise (small objects) and skip comparing if smaller than the biggest area
            perimeter = arcLength(contours[i], true);
            approxPolyDP(contours[i], conPoly, 0.02 * perimeter, true);
            if ((uint8_t)conPoly.size() == 3) {
                biggest_result = shape_t::triangle_s;
                biggest_area = tmp_area;
            }
            else {
                // cout << "contours[i].size() = " << contours[i].size() << ", contourArea(contours[i]) = " << contourArea(contours[i]) << endl << std::flush;
                if (contours[i].size() < 5) continue;       //gir ellipse only works if there are at least 5 points 
                ellipse = fitEllipse(contours[i]); //TODO: convert to float i guess https://docs.opencv.org/4.x/d9/d73/samples_2cpp_2fitellipse_8cpp-example.html


                ellipse.points(ellipse_def_points);
                axis[0] = sqrt(pow((double)ellipse_def_points[0].x - (double)ellipse_def_points[1].x, 2) + pow((double)ellipse_def_points[0].y - (double)ellipse_def_points[1].y, 2));
                axis[1] = sqrt(pow((double)ellipse_def_points[0].x - (double)ellipse_def_points[3].x, 2) + pow((double)ellipse_def_points[0].y - (double)ellipse_def_points[3].y, 2));
                if (axis[1]>axis[0]) {
                    swap(axis[1], axis[0]);
                }
                // cout << "p8.6"<< endl << std::flush;
                eccentricity = sqrt(1- (axis[1] * axis[1])/(axis[0] * axis[0]));
                // cout << "eccentricity = " << eccentricity << endl << std::flush;
                tmp_area_ellipse = (axis[0]/2) * (axis[1]/2) * 3.14;
                // cout << "ellipse area = " << tmp_area_ellipse << ", poly area = " << tmp_area <<  ", proportion = " << tmp_area_ellipse/tmp_area << endl << std::flush;
                if (eccentricity < 0.3 && tmp_area_ellipse/tmp_area > 1 && tmp_area_ellipse/tmp_area < 1.2) {
                    biggest_result = shape_t::circle_s;
                    biggest_area = tmp_area_ellipse;
                }
            }
        }
    }  
    return biggest_result;
}