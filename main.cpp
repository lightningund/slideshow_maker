#include <iostream>
#include <filesystem>
#include <set>
#include <string>
#include <future>
#include <bitset>
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

typedef std::string str;

namespace fs = std::filesystem;
using namespace cv;

constexpr int hash_size{64};
constexpr int hash_bits{hash_size * hash_size};
typedef std::bitset<hash_bits> Hash;

int img_length{3};
int anim_length{5};

int des_width{1920};
int des_height{1080};
double des_ratio{static_cast<double>(des_width) / des_height};

int fps{1};

std::optional<std::string> output_folder;

template <typename Iterator> void compile_folder(Iterator iter);
//Turn all images in a folder into a slideshow for each subfolder
template <typename Iterator> void crop_folder(Iterator iter);
template <typename Iterator> void resize_folder(Iterator iter);
template <typename Iterator> void remove_duplicates(Iterator iter);
void decompile_slideshow(fs::directory_entry file);

str int_to_binary_string(int num) {
	str binary{};
	for (int mask = 1; mask < num; mask <<= 1) {
		binary = ((mask & num) ? "1" : "0") + binary;
	}

	return binary;
}

str intToHex(unsigned long long w, int hexLen) {
	static const char* digits{"0123456789ABCDEF"};
	str rc(hexLen, '0');
	for (int i = 0, j = (hexLen - 1) * 4; i < hexLen; ++i, j -= 4) {
		rc[i] = digits[(w >> j) & 0x0f];
	}
	return rc;
}

str binaryToHex(str bin) {
	static const char* digits{"0123456789ABCDEF"};
	str hex{};
	int startoff{4 - bin.length() % 4};
	for (int i = startoff; i < bin.length(); i += 4) {
		hex += digits[stoi(bin.substr(i, 4))];
	}
	return hex;
}

Hash hash_img(Mat src_img) {
	Mat gray{};
	cvtColor(src_img, gray, COLOR_BGR2GRAY);
	Mat smallGray{};
	resize(gray, smallGray, Size(hash_size + 1, hash_size));
	//Mat hashImg = Mat::zeros(hash_size, hash_size, CV_8UC1);
	Hash hash{};
	for (int y = 0; y < hash_size; y ++) {
		for (int x = 0; x < hash_size; x ++) {
			bool bigger = smallGray.at<int>(x, y) > smallGray.at<int>(x + 1, y);
			hash[y * hash_size + x] = bigger;
			//hashImg.at<int>(Point(x, y)) = bigger ? 255 : 0;
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
	std::vector<std::vector<Point>> contours;
	findContours(thresh, contours, RETR_EXTERNAL, CHAIN_APPROX_SIMPLE); // Find the contours (???)
	Rect coords(0, 0, 0, 0);
	for (int i = 0; i < contours.size(); i ++) {
		std::vector<Point> cnt = contours[i];
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

	double r = w / (double)h;
	int nw = des_width, nh = des_height;

	if (r < des_ratio) {
		nw = static_cast<int>(des_height * r);
	} else if (r > des_ratio) {
		nh = static_cast<int>(des_height / r);
	} else {
		resize(img, img, Size(des_width, des_height));
		return img;
	}

	double xo = (des_width - nw) / 2.0;
	double yo = (des_height - nh) / 2.0;

	resize(img, img, Size(nw, nh));
	Mat new_img = Mat::zeros(des_height, des_width, CV_8UC3);
	Rect roi(xo, yo, nw, nh);
	img.copyTo(new_img(roi));
	return new_img;
}

void crop_file(fs::directory_entry file) {
	str filepath = file.path().string();
	str writepath = filepath;
	if (output_folder.has_value()) writepath = output_folder.value() + "/" + file.path().stem().string() + ".png";
	imwrite(writepath, crop_img(imread(filepath)));
}

void resize_file(fs::directory_entry file) {
	str filepath = file.path().string();
	str writepath = filepath;
	if (output_folder.has_value()) writepath = output_folder.value() + "/" + file.path().stem().string() + ".png";
	std::cout << filepath << " to " << writepath << "\n";
	imwrite(writepath, resize_img(imread(filepath)));
}

bool endswith_list(str string, std::initializer_list<str> lst) {
	for (str substr : lst) {
		if (string.ends_with(substr)) return true;
		return false;
	}
}

bool file_is_img(fs::path file) {
	return (std::set<str>{".jpg", ".jpeg", ".bmp", ".png"}.contains(file.extension().string()));
}

bool file_is_anim(fs::path file) {
	return (std::set<str>{".mov", ".mp4", ".avi"}.contains(file.extension().string()));
}

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

template <typename Iterator> void compile_folder(Iterator iter) {
	if (!output_folder.has_value()) return;

	str enddir = output_folder.value();

	VideoWriter writer;

	str mov_name = "movie";

	writer.open(enddir + mov_name + ".avi", CODEC_MP42, 1, Size(1920, 1080));

	for (fs::directory_entry file : iter) {
		if (!file_is_img(file)) continue;

		str filepath = file.path().string();

		try {
			Mat img = imread(filepath);
			writer.write(resize_img(crop_img(img)));
			remove(filepath.c_str());
		} catch (Exception e) {}
	}

	writer.release();
}

template <typename Iterator> void resize_folder(Iterator iter) {
	std::vector<std::future<void>> futures;
	for (fs::directory_entry file : iter) {
		if(file_is_img(file)) futures.push_back(std::async(crop_file, file));
	}

	for (auto& e : futures) {
		e.get();
	}
}

template <typename Iterator> void crop_folder(Iterator iter) {
	std::vector<std::future<void>> futures;

	for (fs::directory_entry file : iter) {
		if (file_is_img(file)) futures.push_back(std::async(resize_file, file));
	}

	for (auto& e : futures) {
		e.get();
	}
}

template <typename Iterator> void remove_duplicates(Iterator iter) {
	// ?????????????
	// I'll figure this out at some point
	// Probably
}

void decompile_slideshow(fs::directory_entry file) {
	if (!file_is_anim(file)) return;
	if (!output_folder.has_value()) return;

	str filepath = file.path().string();
	VideoCapture cap;
	cap.open(filepath);

	str enddir = output_folder.value();

	Hash last_hash{};
	int frames = 0;
	Mat frame;

	do {
		cap.read(frame);

		Hash frame_hash{hash_img(frame)};
		if(frame_hash == last_hash) continue;
		last_hash = frame_hash;

		str save_path = enddir + std::to_string(frames) + ".png";
		std::cout << frames << "\r";
		imwrite(save_path, frame);

		frames ++;
	} while (!frame.empty());
}

int main(int argc, char* argv[]) {
	argparse::ArgumentParser program("slideshow_maker");

	program.add_argument("-m", "--mode")
		.help("Specify the operating mode. Default: compile.\n\tcompile: Create slideshow from images\n\tcrop: Crop all images\n\tresize: Resize all images to specified size\n\tdecompile: Split movie into frames\n\tduplicates:Remove all identical images")
		.default_value(str("compile"));
	
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
	} catch (const std::runtime_error& err) {
		std::cerr << err.what() << "\n";
		std::cerr << program;
		exit(1);
	}

	output_folder = program.present<str>("-o");

	str input_folder = program.get("-i");

	str mode_arg = program.get("-m");

	if (mode_arg == "compile") {
		if (program["-r"] == true) {
			compile_folder(fs::recursive_directory_iterator(input_folder));
		} else {
			compile_folder(fs::directory_iterator(input_folder));
		}
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
	} else if (mode_arg == "duplicates") {
		if (program["-r"] == true) {
			remove_duplicates(fs::recursive_directory_iterator(input_folder));
		} else {
			remove_duplicates(fs::directory_iterator(input_folder));
		}
	} else if (mode_arg == "decompile") {
		decompile_slideshow(fs::directory_entry(fs::path(input_folder)));
	} else {
		std::cerr << "Invalid Mode\n";
		exit(1);
	}

	img_length = program.get<int>("-il");
	anim_length = program.get<int>("-vl");
	return 0;
}
