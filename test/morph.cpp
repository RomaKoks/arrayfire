#include <gtest/gtest.h>
#include <arrayfire.h>
#include <af/dim4.hpp>
#include <af/traits.hpp>
#include <string>
#include <vector>
#include <testHelpers.hpp>

using std::string;
using std::vector;

template<typename T>
class Morph : public ::testing::Test
{
    public:
        virtual void SetUp() {}
};

// create a list of types to be tested
typedef ::testing::Types<float, double, int, uint, char, uchar> TestTypes;

// register the type list
TYPED_TEST_CASE(Morph, TestTypes);

template<typename inType, bool isDilation, bool isVolume=false>
void morphTest(string pTestFile)
{
    vector<af::dim4>       numDims;
    vector<vector<inType>>      in;
    vector<vector<inType>>   tests;

    readTests<inType,inType,int>(pTestFile, numDims, in, tests);

    af::dim4 dims      = numDims[0];
    af::dim4 maskDims  = numDims[1];
    af_array outArray  = 0;
    af_array inArray   = 0;
    af_array maskArray = 0;
    inType *outData;

    ASSERT_EQ(AF_SUCCESS, af_create_array(&inArray, &(in[0].front()),
                dims.ndims(), dims.get(), (af_dtype)af::dtype_traits<inType>::af_type));
    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &(in[1].front()),
                maskDims.ndims(), maskDims.get(), (af_dtype)af::dtype_traits<inType>::af_type));

    if (isDilation) {
        if (isVolume)
            ASSERT_EQ(AF_SUCCESS, af_dilate3d(&outArray, inArray, maskArray));
        else
            ASSERT_EQ(AF_SUCCESS, af_dilate(&outArray, inArray, maskArray));
    }
    else {
        if (isVolume)
            ASSERT_EQ(AF_SUCCESS, af_erode3d(&outArray, inArray, maskArray));
        else
            ASSERT_EQ(AF_SUCCESS, af_erode(&outArray, inArray, maskArray));
    }

    outData = new inType[dims.elements()];

    ASSERT_EQ(AF_SUCCESS, af_get_data_ptr((void*)outData, outArray));

    for (size_t testIter=0; testIter<tests.size(); ++testIter) {
        vector<inType> currGoldBar = tests[testIter];
        size_t nElems        = currGoldBar.size();
        for (size_t elIter=0; elIter<nElems; ++elIter) {
            ASSERT_EQ(currGoldBar[elIter], outData[elIter])<< "at: " << elIter<< std::endl;
        }
    }

    // cleanup
    delete[] outData;
    ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));
    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));
    ASSERT_EQ(AF_SUCCESS, af_destroy_array(outArray));
}

TYPED_TEST(Morph, Dilate3x3)
{
    morphTest<TypeParam, true>(string(TEST_DIR"/morph/dilate3x3.test"));
}

TYPED_TEST(Morph, Erode3x3)
{
    morphTest<TypeParam, false>(string(TEST_DIR"/morph/erode3x3.test"));
}

TYPED_TEST(Morph, Dilate3x3_Batch)
{
    morphTest<TypeParam, true>(string(TEST_DIR"/morph/dilate3x3_batch.test"));
}

TYPED_TEST(Morph, Erode3x3_Batch)
{
    morphTest<TypeParam, false>(string(TEST_DIR"/morph/erode3x3_batch.test"));
}

TYPED_TEST(Morph, Dilate3x3x3)
{
    morphTest<TypeParam, true, true>(string(TEST_DIR"/morph/dilate3x3x3.test"));
}

TYPED_TEST(Morph, Erode3x3x3)
{
    morphTest<TypeParam, false, true>(string(TEST_DIR"/morph/erode3x3x3.test"));
}

template<typename T, bool isDilation, bool isColor>
void morphImageTest(string pTestFile)
{
    using af::dim4;

    vector<dim4>       inDims;
    vector<string>    inFiles;
    vector<dim_type> outSizes;
    vector<string>   outFiles;

    readImageTests(pTestFile, inDims, inFiles, outSizes, outFiles);

    size_t testCount = inDims.size();

    for (size_t testId=0; testId<testCount; ++testId) {

        af_array inArray  = 0;
        af_array maskArray= 0;
        af_array outArray = 0;
        af_array goldArray= 0;
        dim_type nElems   = 0;

        inFiles[testId].insert(0,string(TEST_DIR"/morph/"));
        outFiles[testId].insert(0,string(TEST_DIR"/morph/"));

        dim4 mdims(3,3,1,1);
        ASSERT_EQ(AF_SUCCESS, af_constant(&maskArray, 1.0,
                    mdims.ndims(), mdims.get(), (af_dtype)af::dtype_traits<T>::af_type));

        ASSERT_EQ(AF_SUCCESS, af_load_image(&inArray, inFiles[testId].c_str(), isColor));
        ASSERT_EQ(AF_SUCCESS, af_load_image(&goldArray, outFiles[testId].c_str(), isColor));
        ASSERT_EQ(AF_SUCCESS, af_get_elements(&nElems, goldArray));

        if (isDilation)
            ASSERT_EQ(AF_SUCCESS, af_dilate(&outArray, inArray, maskArray));
        else
            ASSERT_EQ(AF_SUCCESS, af_erode(&outArray, inArray, maskArray));

        T * outData = new T[nElems];
        ASSERT_EQ(AF_SUCCESS, af_get_data_ptr((void*)outData, outArray));

        T * goldData= new T[nElems];
        ASSERT_EQ(AF_SUCCESS, af_get_data_ptr((void*)goldData, goldArray));

        ASSERT_EQ(true, compareArraysRMSD(nElems, goldData, outData, 0.018f));

        ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));
        ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));
        ASSERT_EQ(AF_SUCCESS, af_destroy_array(outArray));
        ASSERT_EQ(AF_SUCCESS, af_destroy_array(goldArray));
    }
}

TEST(Morph, Grayscale)
{
    morphImageTest<float, true, false>(string(TEST_DIR"/morph/gray.test"));
}

TEST(Morph, ColorImage)
{
    morphImageTest<float, false, true>(string(TEST_DIR"/morph/color.test"));
}

template<typename T, bool isDilation>
void morphInputTest(void)
{
    af_array inArray   = 0;
    af_array maskArray = 0;
    af_array outArray  = 0;

    vector<T>   in(100,1);
    vector<T>   mask(9,1);

    // Check for 4D inputs
    af::dim4 dims(5,5,2,2);
    af::dim4 mdims(3,3,1,1);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &mask.front(),
                mdims.ndims(), mdims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    ASSERT_EQ(AF_SUCCESS, af_create_array(&inArray, &in.front(),
                dims.ndims(), dims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));

    // Check for 1D inputs
    dims = af::dim4(100,1,1,1);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&inArray, &in.front(),
                dims.ndims(), dims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));
}

TYPED_TEST(Morph, DilateInvalidInput)
{
    morphInputTest<TypeParam,true>();
}

TYPED_TEST(Morph, ErodeInvalidInput)
{
    morphInputTest<TypeParam,false>();
}

template<typename T, bool isDilation>
void morphMaskTest(void)
{
    af_array inArray   = 0;
    af_array maskArray = 0;
    af_array outArray  = 0;

    vector<T>   in(100,1);
    vector<T>   mask(16,1);

    // Check for 4D mask
    af::dim4 dims(10,10,1,1);
    af::dim4 mdims(2,2,2,2);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&inArray, &in.front(),
                dims.ndims(), dims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &mask.front(),
                mdims.ndims(), mdims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));

    // Check for 1D mask
    mdims = af::dim4(16,1,1,1);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &mask.front(),
                mdims.ndims(), mdims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));
}

TYPED_TEST(Morph, DilateInvalidMask)
{
    morphMaskTest<TypeParam,true>();
}

TYPED_TEST(Morph, ErodeInvalidMask)
{
    morphMaskTest<TypeParam,false>();
}

template<typename T, bool isDilation>
void morph3DMaskTest(void)
{
    af_array inArray   = 0;
    af_array maskArray = 0;
    af_array outArray  = 0;

    vector<T>   in(1000,1);
    vector<T>   mask(81,1);

    // Check for 2D mask
    af::dim4 dims(10,10,10,1);
    af::dim4 mdims(9,9,1,1);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&inArray, &in.front(),
                dims.ndims(), dims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &mask.front(),
                mdims.ndims(), mdims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate3d(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode3d(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));

    // Check for 4D mask
    mdims = af::dim4(3,3,3,3);

    ASSERT_EQ(AF_SUCCESS, af_create_array(&maskArray, &mask.front(),
                mdims.ndims(), mdims.get(), (af_dtype) af::dtype_traits<T>::af_type));

    if (isDilation)
        ASSERT_EQ(AF_ERR_ARG, af_dilate3d(&outArray, inArray, maskArray));
    else
        ASSERT_EQ(AF_ERR_ARG, af_erode3d(&outArray, inArray, maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(maskArray));

    ASSERT_EQ(AF_SUCCESS, af_destroy_array(inArray));
}

TYPED_TEST(Morph, DilateVolumeInvalidMask)
{
    morph3DMaskTest<TypeParam,true>();
}

TYPED_TEST(Morph, ErodeVolumeInvalidMask)
{
    morph3DMaskTest<TypeParam,false>();
}