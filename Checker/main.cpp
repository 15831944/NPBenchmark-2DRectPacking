#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Visualizer.h"

#include "../Solver/PbReader.h"
#include "../Solver/RectPacking.pb.h"


using namespace std;
//using namespace ck;
using namespace pb;

int main(int argc, char *argv[]) {
    enum CheckerFlag {
        IoError = 0x0,
        FormatError = 0x1,
		CoordinateOverError = 0x2,
        RectangleOverlapError = 0x4,
    };
    string inputPath;
    string outputPath;

    if (argc > 1) {
        inputPath = argv[1];
    } else {
		cerr << "input path: " << flush;
        cin >> inputPath;
    }

    if (argc > 2) {
        outputPath = argv[2];
    } else {
        cerr << "output path: " << flush;
        cin >> outputPath;
    }

    pb::RectPacking::Input input;
    if (!load(inputPath, input)) { return ~CheckerFlag::IoError; }

    pb::RectPacking::Output output;
    ifstream ifs(outputPath);
    if (!ifs.is_open()) { return ~CheckerFlag::IoError; }
    string submission;
    getline(ifs, submission); // skip the first line.
    ostringstream oss;
    oss << ifs.rdbuf();
    jsonToProtobuf(oss.str(), output);

	//check solution.
	int error = 0;
	int bufferEdge = output.length();
	int areaSum = 0;
	if (output.placements().size() != input.rectangles().size()) { error |= CheckerFlag::FormatError; }
	for (auto rect1 = output.placements().begin(); rect1 != output.placements().end(); ++rect1) {
		//每个矩形是否超过缓冲区边界
		
		int width1 = input.rectangles(rect1->id()).width();
		int height1 = input.rectangles(rect1->id()).height();
		if (rect1->rotated){
		//若矩形有旋转则交换高和宽的值
			int t1 = width1; 
			width1 = height1;
			height1 = t1;
		}
		areaSum += width1 * height1;
		int rect1_x = rect1->x(), rect1_y = rect1->y();
		if (rect1_x > bufferEdge || rect1_y > bufferEdge || rect1_x < 0 || rect1_y < 0 ||
			rect1_x + width1 > bufferEdge || rect1_y + height1 > bufferEdge) {
			error |= CheckerFlag::CoordinateOverError;	
			break;
		}
		//每两个矩形之间是否有重叠
		for (auto rect2 = rect1 + 1; rect2 != output.placements().end(); ++rect2) {
			int rect2_id = rect2->id(), rect2_x = rect2->x(), rect2_y = rect2->y();
			int width2 = input.rectangles(rect2_id).width();
			int height2 = input.rectangles(rect2_id).height();
			if (rect2->rotated){
				int t2 = width2;
				width2 = height2;
				height2 = t2;
			}
			if (rect1_x + width1 > rect2_x && rect1_x < rect2_x + width2 ||
				rect1_y + height1 > rect2_y && rect1_y < rect2_y + height2 ) {
				error |= CheckerFlag::RectangleOverlapError;
				break;
			}
		}
	}
	int returnCode = (error == 0) ? output.length() : ~error;
	cout << returnCode << endl;
	return returnCode;

}
