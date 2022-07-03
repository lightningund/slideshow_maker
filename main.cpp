#include <iostream>
#include <filesystem>
#include <set>
#include <string>
#include <future>
#include <windows.h>
#include <unordered_set>
#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <argparse/argparse.hpp>

#define CODEC_MP42 VideoWriter::fourcc('M', 'P', '4', '2')

typedef unsigned long long int hashtype;
typedef unsigned short int smallint;

using namespace cv;
using namespace std;
namespace fs = filesystem;

int img_length = 3, anim_length = 5;

int des_width = 1920, des_height = 1080, folder_limit = 1000;
float des_ratio = des_width / (float)des_height;
int hash_size = 64, fps = 1;

optional<string> output_folder;

string int_to_binary_string(int num) {
	string binary = "";
	for (int mask = 1; mask < num; mask <<= 1) {
		binary = ((mask & num) ? "1" : "0") + binary;
	}

	return binary;
}

string intToHex(hashtype w, int hexLen) {
	static const char* digits = "0123456789ABCDEF";
	string rc(hexLen, '0');
	for (int i = 0, j = (hexLen - 1) * 4; i < hexLen; ++i, j -= 4) {
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

string hash_img(Mat src_img) {
	Mat gray;
	cvtColor(src_img, gray, COLOR_BGR2GRAY);
	Mat smallGray;
	resize(gray, smallGray, Size(hash_size + 1, hash_size));
	Mat hashImg = Mat::zeros(hash_size, hash_size, CV_8UC1);
	string hash = "";
	for (int y = 0; y < hash_size; y++) {
		for (int x = 0; x < hash_size; x++) {
			bool bigger = smallGray.at<int>(x, y) > smallGray.at<int>(x + 1, y);
			hash += bigger ? "1" : "0";
			hashImg.at<int>(Point(x, y)) = bigger ? 255 : 0;
		}
	}

	return hash;
}

Mat crop_img(Mat img) {
	int padding = 5;

	int h = img.rows, w = img.cols;
	Mat big_img = Mat::zeros(h + padding * 2, w + padding * 2, CV_8UC3);
	img.copyTo(big_img(Rect(padding, padding, w, h)));
	Mat gray;
	cvtColor(big_img, gray, COLOR_BGR2GRAY); // Convert the base image into grayscale
	Mat thresh;
	threshold(gray, thresh, 5, 255, THRESH_BINARY); // Run the grayscale through a boolean function (essentially)
	vector<vector<Point>> contours;
	findContours(thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // Find the contours (???)
	Rect coords(0, 0, 0, 0);
	for (int i = 0; i < contours.size(); i++) {
		vector<Point> cnt = contours[i];
		Rect bounds = boundingRect(cnt);
		if (bounds.width > coords.width && bounds.height > coords.height) {
			coords = bounds;
		}
	}

	Mat crop(big_img, coords);
	return crop;
}

Mat resize_img(Mat img) {
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
	Mat new_img = Mat::zeros(des_height, des_width, CV_8UC3);
	Rect roi(xo, yo, nw, nh);
	img.copyTo(new_img(roi));
	return new_img;
}

void crop_file(fs::directory_entry file) {
	string filepath = file.path().string();
	string writepath = filepath;
	if (output_folder.has_value()) writepath = output_folder.value() + "/" + file.path().stem().string() + ".png";
	imwrite(writepath, crop_img(imread(filepath)));
}

void resize_file(fs::directory_entry file) {
	string filepath = file.path().string();
	string writepath = filepath;
	if (output_folder.has_value()) writepath = output_folder.value() + "/" + file.path().stem().string() + ".png";
	cout << filepath << " to " << writepath << endl;
	imwrite(writepath, resize_img(imread(filepath)));
}

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

template <typename Iterator> void slideshow_from_folder(Iterator iter);
//Turn all images in a folder into a slideshow for each subfolder
template <typename Iterator> void crop_folder(Iterator iter);
template <typename Iterator> void resize_folder(Iterator iter);
template <typename Iterator> void remove_duplicates_from_folder(Iterator iter);
void decompile_slideshow(fs::directory_entry file);

//void add_image_for_length(VideoWriter& writer, const Mat& image) {
//	for (int i = 0; i < img_length * fps; i++) {
//		writer.write(image);
//	}
//}
//
//void add_movie(VideoWriter& writer, string file) {
//	VideoCapture video(file);
//	float source_fps = video.get(CAP_PROP_FPS);
//	float total_len = video.get(CAP_PROP_FRAME_COUNT) / source_fps;
//
//	cout << video.get(CAP_PROP_FOURCC) << endl;
//
//	vector<Mat> frames;
//
//	while (true) {
//		Mat frame;
//		video >> frame;
//		if (frame.empty()) break;
//		frames.push_back(resize_img(frame));
//	}
//
//	int total_added_frames = 0;
//	while (total_added_frames < fps * anim_length) {
//		for (int i = 0; i < total_len * fps; i++) {
//			writer.write(frames[i * source_fps / fps]);
//			total_added_frames ++;
//		}
//	}
//
//	video.release();
//	return;
//}
//
//void make_movie(fs::path folder) {
//	string movie_name = folder.string() + "/" + folder.filename().string() + ".mp4";
//	VideoWriter writer(folder.filename().string(), VideoWriter::fourcc('h', '2', '6', '4'), fps, Size(des_width, des_height));
//
//	for (const auto& file : fs::directory_iterator(folder)) {
//		fs::path filepath = file.path();
//		string filename = filepath.string();
//		if (filepath.stem().string() == folder.filename().string()) continue;
//		cout << filename << endl;
//		if (file_is_img(filepath)) {
//			Mat img = imread(filename);
//			img = resize_img(img);
//			add_image_for_length(writer, img);
//			img.release();
//		} else if (file_is_anim(filepath)) {
//			add_movie(writer, filename);
//		}
//	}
//
//	cout << "Done Adding Things!" << endl;
//
//	writer.release();
//
//	return;
//}
//
//void recompile_path() {
//	unordered_set<string> hashes;
//
//	int frames = 0, folders = 0, loops = 0, movies = 0;
//	Mat frame;
//
//	VideoWriter writer;
//	writer.open(enddir + "0.avi", CODEC_MP42, 1, Size(1920, 1080), true);
//
//	for (fs::directory_entry file : fs::recursive_directory_iterator(sourcedir)) {
//		string filepath = file.path().string();
//		string ext = file.path().extension().string();
//		VideoCapture cap;
//
//		try {
//			cap.open(filepath);
//		} catch (exception e) {
//			cout << e.what() << endl;
//			continue;
//		}
//
//		cap.read(frame);
//		Mat newframe = crop_img(frame);
//
//		while (!newframe.empty()) {
//			try {
//				string hash = dhash_img(newframe);
//				cout << hash << endl;
//				if (hashes.find(hash) == hashes.end()) {
//					hashes.insert(hash);
//					writer.write(resize_img(newframe));
//
//					cout << loops << "\r";
//
//					loops++;
//
//					if (loops >= movie_limit) {
//						movies++;
//						loops = 0;
//						writer.release();
//						writer.open(enddir + to_string(movies) + ".avi", CODEC_MP42, 1, Size(1920, 1080), true);
//					}
//				} else {
//					cout << "DUPE" << endl;
//				}
//			} catch (const exception e) {
//				cout << e.what() << endl;
//			}
//		}
//	}
//}

template <typename Iterator> void slideshow_from_folder(Iterator iter) {
	if (!output_folder.has_value()) return;

	string enddir = output_folder.value();

	VideoWriter writer;

	string mov_name = "movie";

	writer.open(enddir + mov_name + ".avi", CODEC_MP42, 1, Size(1920, 1080));

	for (fs::directory_entry file : iter) {
		if (!file_is_img(file)) continue;

		string filepath = file.path().string();

		try {
			Mat img = imread(filepath);
			writer.write(resize_img(crop_img(img)));
			remove(filepath.c_str());
		} catch (Exception e) {}
	}
}

template <typename Iterator> void resize_folder(Iterator iter) {
	vector<future<void>> futures;
	for (fs::directory_entry file : iter) {
		if(file_is_img(file)) futures.push_back(async(crop_file, file));
	}

	for (auto& e : futures) {
		e.get();
	}
}

template <typename Iterator> void crop_folder(Iterator iter) {
	vector<future<void>> futures;

	for (fs::directory_entry file : iter) {
		if (file_is_img(file)) futures.push_back(async(resize_file, file));
	}

	for (auto& e : futures) {
		e.get();
	}
}

template <typename Iterator> void remove_duplicates_from_folder(Iterator iter) {
	// ?????????????
}

void decompile_slideshow(fs::directory_entry file) {
	if (!file_is_anim(file)) return;
	if (!output_folder.has_value()) return;

	string enddir = output_folder.value();

	int frames = 0, folders = 0;
	Mat frame;

	string filepath = file.path().string();
	VideoCapture cap;
	try {
		cap.open(filepath);
	} catch (const exception e) {
		cout << e.what() << endl;
		return;
	}

	do {
		if (frames % folder_limit == 0) {
			frames = 0;
			folders++;
			string folder_path = enddir + to_string(folders) + "/";
			try { CreateDirectoryA(folder_path.c_str(), NULL); } catch (const exception&) {}
		}

		cap.read(frame);
		string save_path = enddir + to_string(folders) + "/" + to_string(frames % folder_limit) + ".png";
		cout << folders << " - " << frames << "\r";
		imwrite(save_path, frame);

		frames++;
	} while (!frame.empty());
}

int main(int argc, char* argv[]) {
	argparse::ArgumentParser program("slideshow_maker");

	program.add_argument("-m", "--mode")
		.help("Specify the operating mode. Default: compile.\n\tcompile: Create slideshow from images\n\tcrop: Crop all images\n\tresize: Resize all images to specified size\n\tdecompile: Split movie into frames\n\tduplicates:Remove all identical images")
		.default_value(string("compile"));
	
	program.add_argument("-i", "--input")
		.help("Specify absolute path to the input folder")
		.required();
	
	program.add_argument("-r", "--recurse")
		.help("Whether to run on all images in all subfolders or only the root folder.")
		.default_value(false)
		.implicit_value(true);

	program.add_argument("-o", "--output")
		.help("Create all the files somewhere else instead. Prevents overwriting if you want to keep the originals.");
	
	program.add_argument("-il", "--image-length")
		.help("How many seconds each image should remain on screen.")
		.default_value(3)
		.scan<'i', int>();

	program.add_argument("-vl", "--video-length")
		.help("The minimum length of time a video should be on screen.")
		.default_value(5)
		.scan<'i', int>();

	try {
		program.parse_args(argc, argv);
	} catch (const runtime_error& err) {
		cerr << err.what() << endl;
		cerr << program;
		exit(1);
	}

	output_folder = program.present<string>("-o");

	string input_folder = program.get("-i");

	string mode_arg = program.get("-m");

	if (mode_arg == "compile") {
	} else if (mode_arg == "crop") {
		if (program["-r"] == true) {
			crop_folder(fs::recursive_directory_iterator(input_folder));
		} else {
			crop_folder(fs::directory_iterator(input_folder));
		}
	} else if (mode_arg == "resize") {
		if (program["-r"] == true) {
			resize_folder(fs::recursive_directory_iterator(input_folder));
		} else {
			resize_folder(fs::directory_iterator(input_folder));
		}
	} else if (mode_arg == "decompile") {
	} else if (mode_arg == "duplicates") {
	} else {
		cerr << "Invalid Mode" << endl;
		exit(1);
	}

	img_length = program.get<int>("-il");
	anim_length = program.get<int>("-vl");
	return 0;
}
