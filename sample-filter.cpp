#include "math.h"
#include <vector>
#include <iostream>
#include <string.h>
#include <assert.h> 
using namespace std;

#include "rgbe.h"

// Read an HDR image in .hdr (RGBE) format.
void read(const string inName, std::vector<float>& image, 
          int& width, int& height)
{
    rgbe_header_info info;
    char errbuf[100] = {0};
    
    // Open file and read width and height from the header
    FILE* fp = fopen(inName.c_str(), "rb");
    if (!fp) {
        printf("Can't open file: %s\n", inName.c_str());
        exit(-1); }
    int rc = RGBE_ReadHeader(fp, &width, &height, &info, errbuf);
    if (rc != RGBE_RETURN_SUCCESS) {
        printf("RGBE read error: %s\n", errbuf);
        exit(-1); }

    // Allocate enough memory
    image.resize(3*width*height);

    // Read the pixel data and close the file
    rc = RGBE_ReadPixels_RLE(fp, &image[0], width, height, errbuf);
    if (rc != RGBE_RETURN_SUCCESS) {
        printf("RGBE read error: %s\n", errbuf);
        exit(-1); }
    fclose(fp);
    
    printf("Read %s (%dX%d)\n", inName.c_str(), width, height);
}

// Write an HDR image in .hdr (RGBE) format.
void write(const string outName, std::vector<float>& image, 
           const int width, const int height)
{
    rgbe_header_info info;
    char errbuf[100] = {0};

    // Open file and write width and height to the header
    FILE* fp  =  fopen(outName.c_str(), "wb");
    int rc = RGBE_WriteHeader(fp, width, height, NULL, errbuf);
    if (rc != RGBE_RETURN_SUCCESS) {
        printf("RGBE write error: %s\n", errbuf);
        exit(-1); }

    // Write the pixel data and close the file
    rc = RGBE_WritePixels_RLE(fp, &image[0], width,  height, errbuf);
    if (rc != RGBE_RETURN_SUCCESS) {
        printf("RGBE write error: %s\n", errbuf);
        exit(-1); }
    fclose(fp);
    
    printf("Wrote %s (%dX%d)\n", outName.c_str(), width, height);
}


int main(int argc, char** argv)
{    
    // Read in-file name from command line, create out-file name
    string inName = argv[1];
    string outName = inName.substr(0,inName.length()-4) + ".irr.hdr";

    int inWidth, inHeight;
    std::vector<float> inImage;
    read(inName, inImage, inWidth, inHeight);

    int outWidth=200, outHeight=100;
    std::vector<float> outImage(3*outWidth*outHeight);

    // Irradiance calculation algorithm:
    //  Loop through all output pixels (The #pragma parallelizes this loop)
#pragma omp parallel for schedule(dynamic, 1) // Magic: Multi-thread y loop
    for (int j=0;  j<outHeight;  j++) {
        for (int i=0;  i<outWidth;  i++) {
            // Calculate N from the indices  i and j

            // For each output pixel, accumulate across *all* input pixels
            for (int l=0;  l<inHeight;  l++) {
                for (int k=0;  k<inWidth;  k++) {
                    // Calculate W from indices k and l
                    // Accumulate input pixel (k,j)'s contribution to the output pixel (i,j)
                }
            }
        }
    }


    // Write the output image
    write(outName, outImage, outWidth, outHeight);
}
