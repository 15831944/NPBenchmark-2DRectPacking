#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

#include "Visualizer.h"

#include "../Solver/PbReader.h"
#include "../Solver/RectPacking.pb.h"


using namespace std;
using namespace szx;
using namespace pb;

int main(int argc, char *argv[]) {
    enum CheckerFlag {
        IoError = 0x0,
        FormatError = 0x1,
		CoordinateOverError = 0x2,
        RectangleOverlapError = 0x4
    };

    string inputPath;
    string outputPath;

    if (argc > 1) {
        inputPath = argv[1];
    } else {
        cout << "input path: " << flush;
        cin >> inputPath;
    }

    if (argc > 2) {
        outputPath = argv[2];
    } else {
        cout << "output path: " << flush;
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

	int error = 0;
	float useRatio = 0;
	int bufferEdge = output.length();
	int areaSum = 0;
	if (output.placements().size() != input.rectangles().size()) { error |= CheckerFlag::FormatError; }
	for (auto rect1 = output.placements().begin(); rect1 != output.placements().end(); ++rect1) {
		int width1 = input.rectangles(rect1->id()).width;
		int height1 = input.rectangles(rect1->id()).height;
		areaSum += width1 * height1;
		if (rect1->x > bufferEdge || rect1->y > bufferEdge || rect1->x < 0 || rect1->y < 0 ||
			rect1->x + width1 > bufferEdge || rect1->y + height1 > bufferEdge) {
			error |= CheckerFlag::CoordinateOverError;	
			break;
		}
		for (auto rect2 = rect1 + 1; rect2 != output.placements().end(); ++rect2) {
			int width2 = input.rectangles(rect2->id).width;
			int height2 = input.rectangles(rect2->id).height;
			if (rect1->x + width1 > rect2->x && rect1->x < rect2->x + width2 ||
				rect1->y + height1 > rect2->y && rect1->y < rect2->y + height2 ) {
				error |= CheckerFlag::RectangleOverlapError;
				break;
			}
		}
	}
	useRatio = areaSum / bufferEdge;
	return (error == 0) ? useRatio : ~error;
	
    // check solution.
	/*
    int error = 0;
    int flightNumOnBridge = 0;
    if (output.assignments().size() != input.flights().size()) { error |= CheckerFlag::FormatError; }
    int f = 0;
    for (auto gate = output.assignments().begin(); gate != output.assignments().end(); ++gate, ++f) {
        // check constraints.
        if ((*gate < 0) || (*gate >= input.airport().gates().size())) { error |= CheckerFlag::FlightNotAssignedError; }
        for (auto ig = input.flights(f).incompatiblegates().begin(); ig != input.flights(f).incompatiblegates().end(); ++ig) {
            if (*gate == *ig) { error |= CheckerFlag::IncompatibleAssignmentError; }
        }
        const auto &flight(input.flights(f));
        for (auto flight1 = input.flights().begin(); flight1 != input.flights().end(); ++flight1) {
            if (*gate != output.assignments(flight1->id())) { continue; }
            int gap = max(flight.turnaround().begin() - flight1->turnaround().end(),
                flight1->turnaround().begin() - flight.turnaround().begin());
            if (gap < input.airport().gates(*gate).mingap()) { error |= CheckerFlag::FlightOverlapError; }
        }

        // check objective.
        if (*gate < input.airport().bridgenum()) { ++flightNumOnBridge; }
    }

    // visualize solution.
    double pixelPerMinute = 1;
    double pixelPerGate = 30;
    int horizonLen = 0;
    for (auto flight = input.flights().begin(); flight != input.flights().end(); ++flight) {
        horizonLen = max(horizonLen, flight->turnaround().end());
    }

    auto pos = outputPath.find_last_of('/');
    string outputName = (pos == string::npos) ? outputPath : outputPath.substr(pos + 1);
    Drawer draw;
    draw.begin("Visualization/" + outputName + ".html", horizonLen * pixelPerMinute, input.airport().gates().size() * pixelPerGate, 1, 0);
    f = 0;
    for (auto gate = output.assignments().begin(); gate != output.assignments().end(); ++gate, ++f) {
        // check constraints.
        if ((*gate < 0) || (*gate >= input.airport().gates().size())) { continue; }
        bool incompat = false;
        for (auto ig = input.flights(f).incompatiblegates().begin(); ig != input.flights(f).incompatiblegates().end(); ++ig) {
            if (*gate == *ig) { incompat = true; break; }
        }
        const auto &flight(input.flights(f));
        draw.rect(flight.turnaround().begin() * pixelPerMinute, *gate * pixelPerGate, 
            (flight.turnaround().end() - flight.turnaround().begin()) * pixelPerMinute, pixelPerGate,
            false, to_string(f), "000000", incompat ? "00c00080" : "4080ff80");
    }
    draw.end();
	
    return (error == 0) ? flightNumOnBridge : ~error;
	*/
}
