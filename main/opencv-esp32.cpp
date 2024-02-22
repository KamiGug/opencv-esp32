#undef EPS
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
#include <esp_err.h>
#include <esp_spiffs.h>
#include <esp_wifi.h>
#include <mdns.h>
#include <microhttpd/microhttpd.h>
#include <esp_camera.h>
#include <sdmmc_cmd.h>
#include "driver/sdmmc_host.h"
#include <esp_vfs_fat.h>


using namespace cv;
using std::cout;
using std::cin;
using std::endl;
using std::string;


//storage consts / variables
#define MOUNT_POINT "/SD"

//numbering variables
#define MAX_SAVED_IMAGES 5;
uint16_t photoNumber = 0; //variable holding number of the last taken photo 

//time variables
uint32_t delayForPhotos; //keep the delay between each photo
uint32_t timestamp; //needed for delay to work well

void test_file() {
    string filename = MOUNT_POINT;
    filename.append("/testfile.jpg");
    cout << "filename: '" << filename << "' filename size: " << filename.length() << endl;
    FILE *f;
    f = fopen(filename.c_str(), "w");
    if (f != NULL) {
        cout << "fopen(filename, \"w\") - OK" << endl;
        fclose(f);
        f = fopen(filename.c_str(), "r");
        if (f != NULL) {
            cout << "fopen(filename, \"r\") - OK" << endl;
            fclose(f);
        }
    }

}





void app_main(void)
{
    setup();
    // test_file();
    parseImage("/SD/PHOTOT.BMP");


    // string debug[] = {
    //     "/SD/PHOTOC.bmp",
    //     "//SD//PHOTOC.bmp",
    //     "//SD//PHOTOC.bmp",
    //     "SD/PHOTOC.bmp",
    //     "SD//PHOTOC.bmp",
    //     "\\SD\\PHOTOC.bmp",
    //     "\\\\SD\\\\PHOTOC.bmp"
    // };

    // for (uint8_t i = 0; i < size(debug); i++)
    // {
    //     Mat to_check = imread(debug[i]);
    //     if (!to_check.empty()) {
    //         cout << "SUCCESS wtih: '" << debug[i] << "'!" << endl;
    //         return;
    //     }
    // }
    // cout << "FAILURE!" <<endl;
    // return;
    // cout << "m1";
    // string fileName = getFileName("t");
    // cout << "m2";
    // shape_t shape = unknown;//parseImage(fileName);
    // cout << "m3";
    // cout << "triangle: " << writeShapeName(shape) << endl;
    // return;
    // cout << "circle: " << writeShapeName(parseImage(getFileName("c"))) << endl;
    // cout << "random: " << writeShapeName(parseImage(getFileName(Uint16ToString(20)))) << endl;

    // // while (true) loop();

    // /* Matrices initialization tests */
    // Mat M1(2,2, CV_8UC3, Scalar(0,0,255));
    // cout << "M1 = " << endl << " " << M1 << endl << endl;

    // Mat M2(2,2, CV_8UC3, Scalar(0,0,111));
    // cout << "M2 = " << endl << " " << M2 << endl << endl;

    // Mat eye = Mat::eye(10, 10, CV_32F) * 0.1;
    // cout << "eye = " << endl << " " << eye << endl << endl;

    // Mat ones = Mat::ones(15, 4, CV_8U)*3;
    // cout << "ones = " << endl << " " << ones << endl << endl;

    // vector<float> v;
    // v.push_back((float)CV_PI);
    // v.push_back(2);
    // v.push_back(3.01f);
    // cout << "floats vector = " << endl << " " << Mat(v) << endl << endl;

    // /* Matrices imgproc operations tests */
    // uint8_t data[15] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    // Mat M3 = Mat(3, 5, CV_8UC1, data);
    // cout << "Gray matrix = " << endl << " " << M3 << endl << endl;

    // Mat M4;
    // threshold(M3, M4, 7, 255, THRESH_BINARY);
    // cout << "Thresholded matrix = " << endl << " " << M4 << endl << endl;

    // Mat M5;
    // resize(M3, M5, Size(), 0.75, 0.75);
    // cout << "Resized matrix = " << endl << " " << M5 << endl << endl;
}

void loop() {
    usleep(500000); //wait for 0.5s
    //auto take image
    //cyclical image numbers
    //parse
}

//setup functions

void setup() {
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
}

bool init_dns() {
    return true;
}

bool init_network() {
    return true;
}

bool init_server() {
    return true;
}

bool init_camera() {
    return true;
}
void critical_failure() {
    while (true) {
        sleep(1);
        cout << "RESET THE BOARD" << endl;
    }
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
string getFileName(string post) {
    string tmp = "";
    tmp.append(MOUNT_POINT);
    tmp.append("/photo");
    tmp.append(post);
    tmp.append(".bmp");
    return tmp;
}



//helpers
string writeShapeName(shape_t input) {
    switch (input) {
        case shape_t::triangle:
            return "triangle";
        case shape_t::circle:
            return "circle";
        case shape_t::unknown:
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

//numbering functions
uint16_t getPictureNumber() {
  return photoNumber;
}

void incPictureNumber() {
    photoNumber = (photoNumber + 1) % MAX_SAVED_IMAGES;
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

//shape detect function
shape_t parseImage(string filepath) {
    // cv::String file = filepath;
    cout << endl << "p1";
    // FILE *f = fopen(file.c_str(), "r");
    // if (f != NULL) {
    //     cout << "opened  with fopen: " << file << endl;
    //     fclose(f);
    // }
    cout << "p2";
    Mat img = imread(filepath);
    cout << "p3";
    Mat imgT;
    // if(img.empty()) {
    //     cout << "Unable to read image: " << file << "!" << endl;
    //     return unknown;
    // }
    // double thresh = 
    threshold(img, imgT, 100, 150, THRESH_BINARY);
    cout << "p4";
    vector<vector<Point>> contours;
    vector<Vec4i> hierarchy;
    findContours(imgT, contours, hierarchy, RETR_TREE, CHAIN_APPROX_SIMPLE);
    cout << "p5";
    cout << "Number of found shapes: " << contours.size() << endl;

    
    if (contours.size() <= 0) {
        return unknown;
    }
    cout << "p6";
    vector<Point> conPoly;                      //polygon that detected in a shape
    RotatedRect ellipse;                        //elipse to check if object is round enough to be a circle
    double axis[2] = {0};
    double eccentricity;
    double tmp_area;
    // vector<Rect> boundRect(contours.size());            
    double biggest_area = 0;
    shape_t biggest_result = unknown;
    Point2f ellipse_def_points[4];
    cout << "p7" << endl;
    for (int i = 0; i < contours.size(); i++) {
        cout << i;
        tmp_area = contourArea(contours[i]);
        if(tmp_area > 200) {    //filter out noise (small objects)
            if (biggest_area > contourArea(contours[i])) continue;   //skip comparisons if it is not the biggest object on the image 
            approxPolyDP(contours[i], conPoly, 0.02 * arcLength(contours, true), true);
            if ((uint8_t)conPoly.size() == 3) {
                biggest_result = shape_t::triangle;
                biggest_area = tmp_area;
            }
            else {
                ellipse = fitEllipse(contours[i]);
                ellipse.points(ellipse_def_points);
                axis[0] = sqrt(pow((double)ellipse_def_points[0].x - (double)ellipse_def_points[1].x, 2) + pow((double)ellipse_def_points[0].y - (double)ellipse_def_points[1].y, 2));
                axis[1] = sqrt(pow((double)ellipse_def_points[0].x - (double)ellipse_def_points[3].x, 2) + pow((double)ellipse_def_points[0].y - (double)ellipse_def_points[3].y, 2));
                if (axis[1]>axis[0]) {
                    swap(axis[1], axis[0]);
                }
                eccentricity = sqrt(1- (axis[1] * axis[1])/(axis[0] * axis[0]));
                if (eccentricity < 0.07) {
                    biggest_result = shape_t::circle;
                    biggest_area = tmp_area;
                }
            }
        }
    }
    return biggest_result;
}
