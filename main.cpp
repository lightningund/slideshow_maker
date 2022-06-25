#include <iostream>
#include <vector>
#include <filesystem>
#include <set>
#include <iostream>
#include <string>
#include <sstream>
#include <future>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>

#define CODEC_MP42 VideoWriter::fourcc('M', 'P', '4', '2')

typedef unsigned long long int hashtype;
typedef unsigned short int smallint;

using namespace cv;
using namespace std;
namespace fs = filesystem;

string ROOT_DIR = "D:/#Pictures/#NSFW/ITF/";
int img_length = 3, mov_length = 5;

float des_width = 1920.0;
float des_height = 1080.0;
float des_ratio = des_width / des_height;

constexpr smallint op_compile = 0;
constexpr smallint op_recompile = 1;
constexpr smallint op_uncompile = 2;
constexpr smallint op_autocrop = 3;
constexpr smallint op_resize = 4;
constexpr smallint op_rename = 5;

smallint operation;
string sourcedir, enddir;

void func_file(fs::directory_entry file, Mat(*f)(Mat));

string intToHex(hashtype w, int hexLen);
string binaryToHex(string binary);
string dhash_img(Mat srcimg);

Mat resize_img(Mat& img) {
	int h = img.rows, w = img.cols;

	if (img.channels() == 4) {
		cvtColor(img, img, COLOR_BGRA2BGR);
	}

	if (w == des_width && h == des_height) return img;

	float r = w / (float)h;
	int nw = des_width, nh = des_height;

	if (r < des_ratio) {
		nw = w * (des_height / (float)h);
	} else if (r > des_ratio) {
		nh = h * (des_height / (float)w);
	} else {
		resize(img, img, Size(des_width, des_height));
		return img;
	}

	float xo = (des_width - nw) / 2.0;
	float yo = (des_height - nh) / 2.0;

	resize(img, img, Size(nw, nh));
	Mat newImg(des_height, des_width, CV_8UC3);
	Rect roi(xo, yo, nw, nh);
	img.copyTo(newImg(roi));
	return newImg;
}

Mat crop_img(Mat srcimg) {
	int width = srcimg.cols;
	int height = srcimg.rows;
	Mat bigImg = Mat::zeros(height * 2, width * 2, CV_8UC3);
	Mat insetImg(bigImg, Rect(width / 2, height / 2, width, height));
	srcimg.copyTo(insetImg);
	Mat gray;
	cvtColor(bigImg, gray, COLOR_BGR2GRAY); // Convert the base image into grayscale
	Mat thresh;
	threshold(gray, thresh, 5, 255, THRESH_BINARY); // Run the grayscale through a boolean function (essentially)
	vector<vector<Point>> contours;
	findContours(thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // Find the contours (???)
	Rect coords(0, 0, width, height);
	for (int i = 0; i < contours.size(); i++) {
		vector<Point> cnt = contours[i];
		Rect bounds = boundingRect(cnt);
		if (bounds.width > coords.width && bounds.height > coords.height) {
			coords = bounds;
		}
	}
	Mat crop(bigImg, coords);
	return crop;
}

void slideshow_from_folder(string folder);
void slideshow_from_subfolders(string folder);
//Turn all images in a folder into a slideshow for each subfolder
void crop_folder(string folder);
void crop_subfolders(string folder);
void resize_folder(string folder);
void resize_subfolders(string folder);
void remove_duplicates_from_folder(string folder);
void remove_duplicates_from_subfolders(string folder);
void decompile_slideshow(string slideshow, string target);

bool endswith_list(string str, initializer_list<string> lst) {
	for (string substr : lst) {
		if (str.ends_with(substr)) return true;
		return false;
	}
}

bool file_is_img(fs::path file) {
	return (set<string>{ ".jpg", ".jpeg", ".bmp", ".png" }.contains(file.extension().string()));
}

bool file_is_anim(fs::path file) {
	return (set<string>{ ".mov", ".mp4", ".avi" }.contains(file.extension().string()));
}

void add_image_for_length(VideoWriter& writer, const Mat& image) {
	for (int i = 0; i < IMAGE_LENGTH * FPS; i++) {
		writer.write(image);
	}
}

void add_movie(VideoWriter& writer, string file) {
	VideoCapture video(file);
	double SOURCE_FPS = video.get(CAP_PROP_FPS);
	double TOTAL_LENGTH = video.get(CAP_PROP_FRAME_COUNT) / SOURCE_FPS;

	cout << video.get(CAP_PROP_FOURCC) << endl;

	vector<Mat> frames;

	while (true) {
		Mat frame;
		video >> frame;
		if (frame.empty()) break;
		frames.push_back(resize_img(frame));
	}

	int total_added_frames = 0;
	while (total_added_frames < FPS * MOVIE_LENGTH) {
		for (int i = 0; i < TOTAL_LENGTH * FPS; i++) {
			writer.write(frames[int(i * SOURCE_FPS / FPS)]);
			total_added_frames++;
		}
	}

	video.release();
	return;
}

void make_movie(fs::path folder) {
	string movie_name = folder.string() + "/" + folder.filename().string() + ".mp4";
	VideoWriter writer(movie_name, VideoWriter::fourcc('h', '2', '6', '4'), FPS, Size(DWIDTH, DHEIGHT));

	for (const auto& file : fs::directory_iterator(folder)) {
		fs::path filepath = file.path();
		string filename = filepath.string();
		if (filepath.stem().string() == folder.filename().string()) continue;
		cout << filename << endl;
		if (file_is_img(filepath)) {
			Mat img = imread(filename);
			img = resize_img(img);
			add_image_for_length(writer, img);
			img.release();
		} else if (file_is_anim(filepath)) {
			add_movie(writer, filename);
		}
	}

	cout << "Done Adding Things!" << endl;

	writer.release();

	return;
}

void compile_img(VideoWriter writer, fs::directory_entry file) {
	string filepath = file.path().string();
	string filename = file.path().filename().string();
	string ext = file.path().extension().string();

	try {
		Mat img = imread(filepath);
		writer.write(resize_img(crop_img(img)));
		remove(filepath.c_str());
	} catch (Exception e) {}
}

void compile_path() {
	VideoWriter writer;
	writer.open(enddir + "0.avi", CODEC_MP42, 1, Size(1920, 1080), true);

	int loops = 0, movies = 0;

	for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
		compile_img(writer, file);
		cout << movies << "-" << loops << "\r";
		loops++;

		if (loops >= movie_limit) {
			movies++;
			loops = 0;
			writer.release();
			writer.open(enddir + to_string(movies) + ".avi", CODEC_MP42, 1, Size(1920, 1080));
		}
	}
}

void recompile_path() {
	unordered_set<string> hashes;

	int frames = 0, folders = 0, loops = 0, movies = 0;
	Mat frame;

	VideoWriter writer;
	writer.open(enddir + "0.avi", CODEC_MP42, 1, Size(1920, 1080), true);

	for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
		string filepath = file.path().string();
		string ext = file.path().extension().string();
		VideoCapture cap;

		try {
			cap.open(filepath);
		} catch (exception e) {
			cout << e.what() << endl;
			continue;
		}

		cap.read(frame);
		Mat newframe = crop_img(frame);

		while (!newframe.empty()) {
			try {
				string hash = dhash_img(newframe);
				cout << hash << endl;
				if (hashes.find(hash) == hashes.end()) {
					hashes.insert(hash);
					writer.write(resize_img(newframe));

					cout << loops << "\r";

					loops++;

					if (loops >= movie_limit) {
						movies++;
						loops = 0;
						writer.release();
						writer.open(enddir + to_string(movies) + ".avi", CODEC_MP42, 1, Size(1920, 1080), true);
					}
				} else {
					cout << "DUPE" << endl;
				}
			} catch (const exception e) {
				cout << e.what() << endl;
			}
		}
	}
}

void apply_user_choice(int operation) {
	switch (operation) {
	case op_compile:
	{
		compile_path();
		break;
	}
	case op_recompile:
	{
		recompile_path();
		break;
	}
	case op_uncompile:
	{
		int frames = 0, folders = 0;
		Mat frame;

		for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
			string filepath = file.path().string();
			string ext = file.path().extension().string();
			VideoCapture cap;
			try {
				cap.open(filepath);
			} catch (const exception e) {
				cout << e.what() << endl;
				continue;
			}

			cap.read(frame);

			string folder = enddir + "0/";
			try { CreateDirectoryA(folder.c_str(), NULL); } catch (const exception&) {}

			while (!frame.empty()) {
				string saveto = enddir + to_string(folders) + "/" + to_string(frames) + ".png";
				cout << folders << "-" << frames << "\r";
				imwrite(saveto, frame);

				cap.read(frame);

				frames++;
				if (frames >= folder_limit) {
					frames = 0;
					folders++;
					string folder = enddir + to_string(folders) + "/";
					try { CreateDirectoryA(folder.c_str(), NULL); } catch (const exception&) {}
				}
			}
		}
		break;
	}
	default:
	{
		switch (operation) {
		case op_autocrop: { func = &crop_img; break; }
		case op_resize: { func = &resize_img; break; }
		case op_rename: { func = &rename_img; break; }
		}
	}
	case op_autocrop: { func = &crop_img; }
	case op_resize:
	{
		if (operation == op_resize) {
			func = &resize_img;
		} else {
			func = &crop_img;
		}
		vector<future<void>> futures;
		for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
			futures.push_back(async(func_file, file, *func));
		}

		for (auto& e : futures) {
			e.get();
		}
		break;
	}
	case op_rename:
	{
		vector<future<void>> futures;
		for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
			futures.push_back(async(rename_img, file));
		}

		for (auto& e : futures) {
			e.get();
		}
		break;
	}
	}
}

void func_file(fs::directory_entry file, Mat(*f)(Mat)) {
	string filepath = file.path().string();
	cout << filepath << "\r";
	try {
		Mat img = imread(filepath);
		imwrite(filepath, f(img));
	} catch (Exception e) { cout << e.msg << endl; }
}

string dhash_img(Mat srcimg) {
	Mat gray;
	cvtColor(srcimg, gray, COLOR_BGR2GRAY);
	Mat smallGray;
	resize(gray, smallGray, Size(hash_size + 1, hash_size));
	Mat hashImg = Mat::zeros(hash_size, hash_size, CV_8UC1);
	string hash = "";
	for (int y = 0; y < hash_size; y++) {
		for (int x = 0; x < hash_size; x++) {
			bool bigger = smallGray.at<uint>(x, y) > smallGray.at<uint>(x + 1, y);
			hash += bigger ? "1" : "0";
			hashImg.at<uint>(Point(x, y)) = bigger ? 255 : 0;
		}
	}

	imwrite("D:/" + to_string(rand() % 1000) + ".png", hashImg);
	return hash;
}

string intToHex(hashtype w, int hexLen) {
	static const char* digits = "0123456789ABCDEF";
	string rc(hexLen, '0');
	for (int i = 0, j = (hexLen - 1) * 4; i < hexLen; ++ i, j -= 4) {
		rc[i] = digits[(w >> j) & 0x0f];
	}
	return rc;
}

string binaryToHex(string bin) {
	static const char* digits = "0123456789ABCDEF";
	string hex = "";
	int startoff = 4 - bin.length() % 4;
	for (int i = startoff; i < bin.length(); i += 4) {
		hex += digits[stoi(bin.substr(i, 4))];
	}
	return hex;
}

int main(int argc, char* argv[]) {
	string movie;

	if (argc < 1) {
		cout << "What do you want to do? 0: Compile, 1: Recompile, 2: Uncompile, 3: Autocrop, 4: Resize, 5: Rename";
		cin >> operation;
	} else {
		operation = stoi(argv[1]);
	}

	if (argc < 2) {
		cout << "Root Directory:";
		cin >> sourcedir;
	} else {
		sourcedir = argv[2];
	}

	if (argc < 3) {
		cout << "Where are you saving to?";
		cin >> enddir;
	} else {
		enddir = argv[3];
	}

	Mat(*func)(Mat);
	void (*onEachFile)(fs::directory_entry);

	cout << operation << endl;

	apply_user_choice(operation);

	return 0;
}
