#include <iostream>
#include <vector>
#include <random>

#include "slic.h"

#include <opencv2/opencv.hpp>

typedef int64_t s64;

using std::cout;
using std::endl;

using std::vector;
using std::string;

using cv::imshow;
using cv::imread;
using cv::Mat;
using cv::Scalar;
using cv::Point;
using cv::Rect;
using cv::Vec3b;
using cv::Size;

const int patchRadius = 3;
const int sideLength = 2 * patchRadius + 1;

void printTimeSinceLastCall(const char* message)
{
	static s64 freq = static_cast<int>(cv::getTickFrequency());
	static s64 last = cv::getTickCount();

	s64 curr = cv::getTickCount();
	s64 delta = curr - last;
	// double deltaMs = (double)delta / freq * 1000;
	double deltaSeconds = (double)delta / freq;

	// printf("%s: %.2f %.2f\n", message, deltaMs, deltaSeconds);
	printf("%s: %.2f\n", message, deltaSeconds);

	last = curr;
}

Vec3b ConvertLabelToColor(int label)
{
    if(label == 0)
        return Vec3b(0, 0, 0);
    if(label == 1)
        return Vec3b(255, 0, 0);
    if(label == 2)
        return Vec3b(0, 255, 0);
    if(label == 3)
        return Vec3b(0, 0, 255);
    if(label == 4)
        return Vec3b(255, 255, 0);
    if(label == 5)
        return Vec3b(255, 0, 255);
    if(label == 6)
        return Vec3b(0, 255, 255);
    if(label == 7)
        return Vec3b(128, 128, 128);
    if(label == 8)
        return Vec3b(255, 255, 255);
}


Mat FragmentToColorDistVisualization(Mat image, Mat imageLabels, vector<vector<double>> distances, int label)
{
    Mat vizImage = Mat::zeros(cv::Size(image.cols, image.rows), CV_8UC3);

    for(int i = 0; i < imageLabels.rows; i++)
    {
        for(int j = 0; j < imageLabels.cols; j++)
        {
            Vec3b pixelColor = image.at<Vec3b>(i , j);
            double dist = distances[imageLabels.at<int>(i , j)][label];
            uchar a = round(pixelColor[0] * dist);
            uchar b = round(pixelColor[1] * dist);
            uchar c = round(pixelColor[2] * dist);

            vizImage.at<Vec3b>(i , j) = Vec3b(a, b, c);
        }
    }

    // vector<bool> imageLabels(size.)

    // for(int i = 0; i < imageLabels.rows; i++)
    // {
    //     for(int j = 0; j < imageLabels.cols; j++)
    //     {
    //         string text = (string)dist;
    //         int fontFace = FONT_HERSHEY_SCRIPT_SIMPLEX;
    //         double fontScale = 2;
    //         int thickness = 3;
    //         cv::Point textOrg(10, 130);
    //         cv::putText(img, text, textOrg, fontFace, fontScale, Scalar::all(255), thickness,8);

    //         Vec3b pixelColor = image.at<Vec3b>(i , j);
    //         double dist = distances[imageLabels.at<int>(i , j)][label];
    //         uchar a = round(pixelColor[0] * dist);
    //         uchar b = round(pixelColor[1] * dist);
    //         uchar c = round(pixelColor[2] * dist);

    //         vizImage.at<Vec3b>(i , j) = Vec3b(a, b, c);
    //     }
    // }

    return vizImage;
}

Mat PaintLabelsTrainImage(Mat imageLabels)
{
    imageLabels.convertTo(imageLabels, CV_32S);
    Mat newImage = Mat::zeros(cv::Size(imageLabels.cols, imageLabels.rows), CV_8UC3);
    for(int i = 0; i < imageLabels.rows; i++)
    {
        for(int j = 0; j < imageLabels.cols; j++)
        {
            newImage.at<Vec3b>(i , j) = ConvertLabelToColor(imageLabels.at<int>(i , j));
        }
    }
    return newImage;
}

Mat PaintInAverageColor(Mat image, Mat imageLabels)
{
    const int   imageRows = image.rows,
                imageCols = image.cols;
    double      maxLabel,
                minLabel;

    minMaxLoc(imageLabels, &minLabel, &maxLabel);
    vector<vector<Vec3b>> pixelValues(maxLabel + 1);
    for(int i = 0; i < imageRows; i++)
    {
        for(int j = 0; j < imageCols; j++)
        {
            pixelValues[imageLabels.at<int>(i , j)].push_back(image.at<Vec3b>(i, j));
        }
    }

    vector<int> sumA(maxLabel + 1, 0);
    vector<int> sumB(maxLabel + 1, 0);
    vector<int> sumC(maxLabel + 1, 0);

    vector<int> howMany(maxLabel + 1, 0);

    for(int i = 0; i < pixelValues.size(); i++)
    {
        for(int j = 0; j < pixelValues[i].size(); j++)
        {
            sumA[i] += pixelValues[i][j][0];
            sumB[i] += pixelValues[i][j][1];
            sumC[i] += pixelValues[i][j][2];
            howMany[i]++;
        }
    }

    vector<Vec3b> avgPixels(maxLabel + 1);
    for(int i = 0; i < pixelValues.size(); i++)
    {
        avgPixels[i][0] = sumA[i] / howMany[i];
        avgPixels[i][1] = sumB[i] / howMany[i];
        avgPixels[i][2] = sumC[i] / howMany[i];
    }

    // vector<Vec3b> pixelAvg(maxLabel + 1);

    // for(int i = 0; i < maxLabel + 1; i++)
    // {
    //     Vec3b sum = std::accumulate(pixelValues[i].begin(), pixelValues[i].end(), 0);
    //     // Vec3b avg = sum / pixelValues[i].size();
    //     // pixelAvg[i] = avg;
    // }

    Mat newImage = Mat::zeros(cv::Size(image.cols, image.rows), CV_8UC3);

    for(int i = 0; i < imageRows; i++)
    {
        for(int j = 0; j < imageCols; j++)
        {
            newImage.at<Vec3b>(i , j) = avgPixels[imageLabels.at<int>(i , j)];
        }
    }

    return newImage;
}

vector<vector<Rect>> RandomPatchesForEachLabel(Mat image, Mat imageLabels)
{
    // Defining some usful constants,
    // finding the minimum and maximum values of the labeling.
    imageLabels.convertTo(imageLabels, CV_32S);

    const int   imageRows = image.rows,
                imageCols = image.cols;
    double      maxLabel,
                minLabel;

    minMaxLoc(imageLabels, &minLabel, &maxLabel);

    ////
    // Step 2: Choose several random pixels for each fregment
    ////

    // First, we calculate how many pixels each label has.
    // We do this to know how many repesentatives we should pick for each label.

    // axis shceme for the image, we choose this because of OpenCV's "at" function.

    // -x
    // ^
    // |
    // |
    // |
    // ------> y

    vector<vector<Point>> pixelsLabeled(maxLabel + 1);

    for(int i = 0; i < imageRows; i++)
    {
        for(int j = 0; j < imageCols; j++)
        {
            // cout << i << " " << j <<  " " << labeledImage.at<int>(i , j) << endl;
            pixelsLabeled[imageLabels.at<int>(i , j)].push_back(Point(i, j));
        }
    }

    // cout << pixelsLabeled.size() << endl;

    // Choose sqrt(pointsLabeldWith(i)) points randomally

    std::random_device rd;
    std::mt19937 gen(rd());
    vector<vector<Point>> patchRepresentatives(maxLabel + 1);

    for(int i = minLabel; i < maxLabel + 1; i++)
    {
        int currentSize = pixelsLabeled[i].size();
        int toChoose = round(sqrt(currentSize) / 2);
        std::uniform_int_distribution<> dis(0, currentSize - 1);

        for(int j = 0; j < toChoose; j++)
        {
            patchRepresentatives[i].push_back( pixelsLabeled[i][dis(gen)] );
        }
    }

    ////
    // Step 3: Determine patches, search for closets patch in the example image.
    ////

    // We have to maintain the same size for all patches,
    // therefore, if one patch exceeds the image limits,
    // we will just ignore it.

    // Rect is defined by its top left corner (x, y), width and height
    // (x,y) ------
    // |
    // |
    // |
    // |
    // |

    int sideLength = 2 * patchRadius + 1;
    vector<vector<Rect>> patches(maxLabel + 1);

    for(int i = minLabel; i < maxLabel + 1; i++)
    {
        for(Point p : patchRepresentatives[i])
        {
            if( p.x - patchRadius >= 0
            &&  p.y - patchRadius >= 0
            &&  p.x + patchRadius < imageRows
            &&  p.y + patchRadius < imageCols)
            {
                patches[i].push_back(Rect(p.x - patchRadius, p.y - patchRadius, sideLength, sideLength));
            }
        }
    }

    return patches;
}

// TODO: Check this function!!! likes to give exeptions. maybe its Vec3b?
// Fixed. Probalby the problem was with acssesing outside the matrix...

Mat VisualizePatches(Mat image, vector<vector<Rect>> patches, int patchLabel = -1)
{
    Mat vizPatch = Mat::zeros(cv::Size(image.cols, image.rows), CV_8UC3);

    for(int i = 0; i < patches.size(); i++)
    {
        if(patchLabel == i || patchLabel == -1)
        {
            for(Rect rec : patches[i])
            {
                for(int i = rec.x; i < rec.x + sideLength; i++)
                {
                    for(int j = rec.y; j < rec.y + sideLength; j++)
                    {
                        if( i < image.rows && j < image.cols )
                            vizPatch.at<cv::Vec3b>(i,j) = image.at<cv::Vec3b>(i,j);
                    }
                }
            }
        }
    }

    return vizPatch;
}

double subSquare(uchar a, uchar b)
{
    return (a - b) * (a - b);
}

double Cie76Compare(Vec3b first, Vec3b second)
{
    double differences =    subSquare(first[0], second[0])
                            +  subSquare(first[1], second[1])
                            +  subSquare(first[2], second[2]);
    return sqrt(differences);
}


Mat drawBoarderFromLabels(Mat image, Mat imageLabels)
{
    imageLabels.convertTo(imageLabels, CV_32S);
    Mat newImage = Mat::zeros(cv::Size(image.cols, image.rows), CV_8UC3);

    int imageCols = image.cols;
    int imageRows = image.rows;

    for(int i = 1; i < imageRows - 1; i++)
    {
        for(int j = 1; j < imageCols - 1; j++)
        {
            bool boarderPixel = false;
            for(int x = -1; x < 2; x++)
            {
                for(int y = -1; y < 2; y++)
                {
                    if( imageLabels.at<int>(i + x , j + y) != imageLabels.at<int>(i , j) )
                        boarderPixel = true;
                }
            }
            if(boarderPixel)
                newImage.at<cv::Vec3b>(i, j) = cv::Vec3b(0, 0, 255);
            else
                newImage.at<cv::Vec3b>(i, j) = image.at<cv::Vec3b>(i, j);
        }
    }

    return newImage;
}

Mat subtructFregmentAverageColor(Mat image, Mat imageLabels)
{
    Mat newImage = Mat::zeros(cv::Size(image.cols, image.rows), CV_8UC3);

    const int   imageRows = image.rows,
                imageCols = image.cols;
    double      maxLabel,
                minLabel;

    minMaxLoc(imageLabels, &minLabel, &maxLabel);

    vector<vector<Point>> pixelsLabeledPoints(maxLabel + 1);
    vector<vector<int>> pixelsLabeledValues(maxLabel + 1);

    for(int i = 0; i < imageRows; i++)
    {
        for(int j = 0; j < imageCols; j++)
        {
            pixelsLabeledPoints[imageLabels.at<int>(i , j)].push_back(Point(i, j));
            pixelsLabeledValues[imageLabels.at<int>(i , j)].push_back(image.at<Vec3b>(i, j)[0]);
            pixelsLabeledValues[imageLabels.at<int>(i , j)].push_back(image.at<Vec3b>(i, j)[1]);
            pixelsLabeledValues[imageLabels.at<int>(i , j)].push_back(image.at<Vec3b>(i, j)[2]);
        }
    }

    for(int i = 0; i < pixelsLabeledPoints.size(); i++)
    {
        int fregmentAverage = (int) (accumulate( pixelsLabeledValues[i].begin(), pixelsLabeledValues[i].end(), 0.0) / pixelsLabeledValues[i].size() );
        for(Point p : pixelsLabeledPoints[i])
        {
            Vec3b currentColor = image.at<Vec3b>(p.x , p.y);
            int a = currentColor[0] - fregmentAverage;
            int b = currentColor[1] - fregmentAverage;
            int c = currentColor[2] - fregmentAverage;
            newImage.at<Vec3b>(p.x , p.y) = Vec3b(a, b, c);
        }
    }

    return newImage;
}

void usage()
{
    std::cout << "\n Usage: hw1 [FILE_NAME]\n"
	      << '\n'
	      << "Please make sure you have the following files in the images folder: \n"
	      << "\"[FILE_NAME]_train\" \"[FILE_NAME]_test\" with .jpg or .tif endings. \n"
          << "Also, \"[FILE_NAME]_train_labels.tif\" should be placed there. \n"
          << "Good luck! \n"
          << endl;
}

// TODO: throw an exepction?

Mat tryLoadingImage(string path)
{
    string jpgEnding = ".jpg";
    string tifEnding = ".tif";

    Mat image = cv::imread(path + jpgEnding);
    if(! image.data )
    {
        image = cv::imread(path + tifEnding);
        if(! image.data )
        {
            cout << "\n Please make sure \"" + path + "\" is located in the right location. \n";
        }
    }

    return image;
}

int main(int argc, char *argv[])
{
    // Reciving user input for the image.

    string folder = "../images/";
    string testString = "_test";
    string trainString = "_train";
    string trainLabelsString = "_train_labels.tif";
    string fileName;

    if (argc != 2)
    {
		usage();
		return 1;
    }
    else
    {
        fileName = (string)argv[1];
    }

    // Mat trainImage = cv::imread("../images/kayak_train.jpg");
    // Mat trainLabels = cv::imread("../images/kayak_train_labels.tif", CV_LOAD_IMAGE_GRAYSCALE);
    // Mat testImage = cv::imread("../images/kayak_test.jpg");

    string trainImagePath = folder + fileName + trainString;
    string trainLabelsPath = folder + fileName + trainLabelsString;
    string testImagePath = folder + fileName + testString;

    Mat trainImage = tryLoadingImage(trainImagePath);
    Mat trainLabels = cv::imread(trainLabelsPath, CV_LOAD_IMAGE_GRAYSCALE);
    Mat testImage = tryLoadingImage(testImagePath);

    Mat myImage = PaintLabelsTrainImage(trainLabels);

    SLIC slic;
    int estSuperpixelsNum = 1000;

    ////
    // Step 1: Compute input image fragments
    ////

    // Find the Super Pixels in the image. The parameter is defined above
    // The GetLabel method gives us 1-dim array with the pixel laybeling.

    printTimeSinceLastCall("Generate Super Pixels");

    slic.GenerateSuperpixels(testImage, estSuperpixelsNum);
    Mat superPixels = slic.GetImgWithContours(Scalar(0, 0, 255));
    int* label = slic.GetLabel();

    // Translation of the array into a Mat object, the same size as the image
    // only with label number insted of pixel values.

    Mat labeledImage = Mat::zeros(cv::Size(testImage.cols, testImage.rows), CV_32S);
    int imageRows = labeledImage.rows;
    int imageCols = labeledImage.cols;
    for(int i = 0; i < imageRows; i++)
    {
        for(int j = 0; j < imageCols; j++)
        {
            labeledImage.at<int>(i , j) = label[i * imageCols + j];
        }
    }

    // Utility: shows the superpixels formed on the image.

    // Mat showMeThePixels = drawBoarderFromLabels(testImage, labeledImage);
    // cv::imshow("w", showMeThePixels);
    // cv::waitKey(0);

    ////
    // Step 2 + 3 combined, see RandomPatchesForEachLabel
    ////

    printTimeSinceLastCall("Random patches test");

    vector<vector<Rect>> testPatches = RandomPatchesForEachLabel(testImage, labeledImage);

    // Utility: show the chosen patches.

    // if called as VisualizePatches( , ) will show all patches,
    // if called as VisualizePatches( , , l) will show chosen for label l.

    // Mat vizTestPatch = VisualizePatches(testImage, testPatches, 200);
    // cv::imshow("w", vizTestPatch);
    // cv::waitKey(0);

    printTimeSinceLastCall("Random patches train");

    // Patches for the training image
    vector<vector<Rect>> trainPatches = RandomPatchesForEachLabel(trainImage, trainLabels);

    // Mat vizTrainPatch = VisualizePatches(trainImage, trainPatches);
    // cv::imshow("w", vizTrainPatch);
    // cv::waitKey(0);

    ////
    // Step 3
    ////

    // Calulation of color difference for patches.
    // We use the CIE76 method to do this (CIE 1976)

    Mat trainImageLab(cv::Size(trainImage.cols, trainImage.rows), CV_8UC3);
    Mat testImageLab(cv::Size(testImage.cols, testImage.rows), CV_8UC3);

    // Test patch, Color, the according distances

    // TODO: look at this. Just trying things out and hoping for the best...
    // testImage = subtructFregmentAverageColor(testImage, labeledImage);
    // trainImage = subtructFregmentAverageColor(trainImage, trainLabels);

    cvtColor(trainImage, trainImageLab, CV_BGR2Lab);
    cvtColor(testImage, testImageLab, CV_BGR2Lab);

    printTimeSinceLastCall("Calculate patch distances");

    // The calculation of the distance of each patch to each of the labels.
    //

    vector<vector<vector<double>>> distancePerPixel(testPatches.size(), vector<vector<double>>(trainPatches.size()));

    for(int i = 0; i < testPatches.size(); i++)
    {
        for(Rect testSquare : testPatches[i])
        {
            for(int j = 0; j < trainPatches.size(); j++)
            {
                double smallestDistance = DBL_MAX;
                double currentDistance;
                double smallestColorDistance = DBL_MAX;

                for(Rect trainSquare : trainPatches[j])
                {
                    double currenPatchDistance = 0;
                    for(int x = 0; x < sideLength; x++)
                    {
                        for(int y = 0; y < sideLength; y++)
                        {
                            Vec3b trainPixel = trainImageLab.at<Vec3b>(trainSquare.x + x, trainSquare.y + y);
                            Vec3b testPixel = testImageLab.at<Vec3b>(testSquare.x + x, testSquare.y + y);
                            currenPatchDistance += Cie76Compare(trainPixel, testPixel);
                        }
                    }
                    if(currenPatchDistance < smallestColorDistance)
                    {
                        smallestColorDistance = currenPatchDistance;
                    }
                }
                distancePerPixel[i][j].push_back(smallestColorDistance);
            }
        }
    }

    printTimeSinceLastCall("Calculate fregment distances");

    vector<vector<double>> fragmentDistance(testPatches.size(), vector<double>(trainPatches.size()));
    for(int i = 0; i < testPatches.size(); i++)
    {
        for(int j = 0; j < trainPatches.size(); j++)
        {
            // Choose the median value of all minimum distances
            // TODO: Check this! sometimes when = 0, segmentation fault. fuck!
            if(distancePerPixel[i][j].size() != 0)
            {
                // cout << distancePerPixel[i][j].size() << "i " << i << " j " << j << endl;
                std::nth_element(   distancePerPixel[i][j].begin(),
                                    distancePerPixel[i][j].begin() + distancePerPixel[i][j].size() / 2,
                                    distancePerPixel[i][j].end());

                fragmentDistance[i][j] = distancePerPixel[i][j][distancePerPixel[i].size() / 2];
            }
        }
    }

    printTimeSinceLastCall("Normalize Distances");

    // Normalize
    double  maxVal = -1,
            minVal = DBL_MAX;

    for(int i = 0; i < fragmentDistance.size(); i++)
    {
        for(int j = 0; j < trainPatches.size(); j++)
        {
            if(fragmentDistance[i][j] > maxVal)
            {
                maxVal = fragmentDistance[i][j];
            }
            if(fragmentDistance[i][j] < minVal)
            {
                minVal = fragmentDistance[i][j];
            }
        }
    }
    vector<vector<double>> normalizedFregmentColorDistance(testPatches.size(), vector<double>(trainPatches.size()));

    for(int i = 0; i < fragmentDistance.size(); i++)
    {
        for(int j = 0; j < trainPatches.size(); j++)
        {
            normalizedFregmentColorDistance[i][j] =
                        (fragmentDistance[i][j] - minVal) /
                        (maxVal - minVal);
        }
    }

    printTimeSinceLastCall("Normalize each fregment distance");

    // Normalize each fragment distance

    for(int i = 0; i < testPatches.size(); i++)
    {
        maxVal = -1;
        minVal = DBL_MAX;

        for(int j = 0; j < trainPatches.size(); j++)
        {
            if(normalizedFregmentColorDistance[i][j] > maxVal)
            {
                maxVal = normalizedFregmentColorDistance[i][j];
            }
            if(normalizedFregmentColorDistance[i][j] < minVal)
            {
                minVal = normalizedFregmentColorDistance[i][j];
            }
        }
        for(int j = 0; j < trainPatches.size(); j++)
        {
            normalizedFregmentColorDistance[i][j] =
                        (normalizedFregmentColorDistance[i][j] - minVal) /
                        (maxVal - minVal);
        }
    }

    // Mat vizFrag = FragmentToColorDistVisualization(testImage, labeledImage, normalizedFregmentColorDistance, 0);
    // //Mat vizPatch = VisualizePatches(trainImage, trainPatches, 0);
    // Mat justShowTheLabeles = PaintLabelsTrainImage(trainLabels);
    // imshow("w", justShowTheLabeles);
    // imshow("ww", vizFrag);
    // //imshow("www", vizPatch);
    // cv::waitKey(0);

    // for(int i = 0; i < fragmentDistance.size(); i++)
    // {
    //     for(int j = 0; j < trainPatches.size(); j++)
    //     {
    //         cout << normalizedFregmentColorDistance[i][j] << " ";
    //     }
    //     cout << endl;
    // }
    // cout << maxVal << " " << minVal << endl;

    ////
    // Step 4: we now have the mapping! huryy!!!!
    ////

    Mat avgColoredImage = PaintInAverageColor(testImage, labeledImage);
    Mat countVotingsForPixel = Mat::zeros(cv::Size(avgColoredImage.cols, avgColoredImage.rows), CV_8U);
    Mat finalLabeling = Mat::zeros(cv::Size(avgColoredImage.cols, avgColoredImage.rows), CV_8U);

    vector<Mat> foregroundImages(trainPatches.size());

    for(int labelNumber = 0; labelNumber < trainPatches.size(); labelNumber++)
    {
        Mat grabCutMask = Mat::zeros(cv::Size(avgColoredImage.cols, avgColoredImage.rows), CV_8U);

        for(int i = 0; i < avgColoredImage.rows; i++)
        {
            for(int j = 0; j < avgColoredImage.cols; j++)
            {
                double currentLable = labeledImage.at<int>(i, j);
                double currentCut = normalizedFregmentColorDistance[currentLable][labelNumber];

                if(currentCut < 0.05)
                    grabCutMask.at<uchar>(i, j)  = cv::GC_PR_FGD;
                else if(currentCut >= 0.05)
                    grabCutMask.at<uchar>(i, j)  = cv::GC_PR_BGD;

            }
        }

        string grabCutString = "Grab Cut " + std::to_string(labelNumber);

        printTimeSinceLastCall(grabCutString.c_str());

        Mat background;
        Mat foreground;
        grabCut(avgColoredImage, grabCutMask, Rect(1, 1, 480, 640), background, foreground, 8);
        cv::compare(grabCutMask, cv::GC_PR_FGD, grabCutMask, cv::CMP_EQ);
        // This sets pixels that are equal to 255.

        foregroundImages[labelNumber] = Mat(avgColoredImage.size(), CV_8UC3, Scalar(255,255,255));
        avgColoredImage.copyTo(foregroundImages[labelNumber], grabCutMask);

        // Sometimes label can spread on most of the picture,
        // making all pixels decide by minimum weight insted of
        // the help of the grabCut algorithem.

        int pixelsAssignedToLabel = countNonZero(grabCutMask);
        int totalPixels = avgColoredImage.cols * avgColoredImage.rows;

        // TODO: is this a good thrshold?
        if(pixelsAssignedToLabel < 0.8 * totalPixels)
        {
            for(int i = 0; i < avgColoredImage.rows; i++)
            {
                for(int j = 0; j < avgColoredImage.cols; j++)
                {
                    if(grabCutMask.at<uchar>(i, j) == 255)
                    {
                        finalLabeling.at<uchar>(i, j) = labelNumber;
                        countVotingsForPixel.at<uchar>(i, j)++;
                    }
                }
            }
        }
        else
        {
            foregroundImages[labelNumber].col(avgColoredImage.cols / 2).setTo(Vec3b(0,0,255));
            foregroundImages[labelNumber].row(avgColoredImage.rows / 2).setTo(Vec3b(0,0,255));
        }


        // Mat finalVizi = PaintLabelsTrainImage(finalLabeling);
        // imshow("w", finalVizi);
        // imshow("ww", grabCutMask);
        // cv::waitKey(0);
    }

    printTimeSinceLastCall("Final Labeling");

    for(int i = 0; i < avgColoredImage.rows; i++)
    {
        for(int j = 0; j < avgColoredImage.cols; j++)
        {
            if(countVotingsForPixel.at<uchar>(i, j) != 1)
            {
                int index = 0;
                for(int k = 0; k < trainPatches.size(); k++)
                {
                    if( normalizedFregmentColorDistance[labeledImage.at<int>(i, j)][k] <
                        normalizedFregmentColorDistance[labeledImage.at<int>(i, j)][index] )
                        {
                            index = k;
                        }
                }
                finalLabeling.at<uchar>(i, j) = index;
            }
        }
    }

    string windowName = "www";

    for(int i = 0; i < trainPatches.size(); i++)
    {
        for(int j = 0; j < i; j++)
            windowName += "w";
        imshow(windowName, foregroundImages[i]);
    }

    //finalLabeling.convertTo(finalLabeling, CV_32S);
    Mat finalViz = PaintLabelsTrainImage(finalLabeling);
    Mat finalVizBoarder = drawBoarderFromLabels(testImage, finalLabeling);

    imshow("w", finalViz);
    imshow("ww", finalVizBoarder);

    // int elementN = 4;
    // Mat element = getStructuringElement(cv::MORPH_RECT, Size(elementN*2+1, elementN*2+1), Point(elementN, elementN));
	// morphologyEx(finalLabeling, finalLabeling, cv::MORPH_CLOSE, element);
    // finalVizBoarder = drawBoarderFromLabels(testImage, finalLabeling);
    // imshow("www", finalVizBoarder);

    // imshow("ww", trainImage);
    // imshow("www", testImage);

    // imshow("w", trainLabels);

    //imshow("w", vizTrainPatch);
    //imshow("ww", trainImage);
    //imshow("www", avgColoredImage);
    while(cv::waitKey(0) != 'q')
    {

    }

    //imshow("w", vizTrainPatch);
    // Step 1 : computing input image fragments

    return 1;
}

//    // Proves it works
//     for(int i = 0; i < testImage.rows; i++)
//     {
//         for(int j = 0; j < testImage.cols; j++)
//         {
//             cout << labled.at<int>(i , j) << endl;
//         }
//     }

//     Mat labled = Mat::zeros(cv::Size(640, 480), CV_8U);
//     for(int i = 0; i < labled.rows; i++)
//     {
//         for(int j = 0; j < labled.cols; j++)
//         {
//             labled.at<uchar>(i , j) = label[i * labled.cols + j];
//         }
//     }