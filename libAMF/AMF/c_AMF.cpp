/********************************************************
 * c_AMF.cpp
 * C++ implements on AMF
 * Author: Jamie Zhu <jimzhu@GitHub>
 * Created: 2014/5/6
 * Last updated: 2016/02/15
********************************************************/

#include <iostream>
#include <cstring>
#include <cmath>
#include <vector>
#include <utility>
#include <algorithm>
#include <ctime>
#include <cstdlib>
#include "c_AMF.h"
using namespace std;

const double EPS = 1e-8;


inline double sqr(double x) {return x * x;}

/********************************************************
 * Udata, Sdata, predData are the output values
********************************************************/
void AMF(double *removedData, int numUser, int numService, int dim, double lmda, 
    int maxIter, double convergeThreshold, double eta, double beta, bool debugMode, 
    double *Udata, double *Sdata, double *predData)
{   
    // --- transfer the 1D pointer to 2D array pointer
    double **removedMatrix = vector2Matrix(removedData, numUser, numService);
    double **U = vector2Matrix(Udata, numUser, dim);
    double **S = vector2Matrix(Sdata, numService, dim);
    double **predMatrix = vector2Matrix(predData, numUser, numService);

    // --- create 2D matrix
    double **lastU = createMatrix(numUser, dim);
    double **lastS = createMatrix(numService, dim);
    copyMatrix(lastU, U, numUser, dim);
    copyMatrix(lastS, S, numService, dim);

    // --- transform removedMatrix into samples
    vector<pair<int, int> > spIndex;
    vector<double> spValue;   
    int numSample = 0;
    for (int i = 0; i < numUser; i++) { 
        for (int j = 0; j < numService; j++) {       
            if (fabs(removedMatrix[i][j]) > EPS) {
                spIndex.push_back(make_pair(i, j));
                spValue.push_back(removedMatrix[i][j]);
                numSample++;
            }
        }
    }

    // --- iterate by standard gradient descent algorithm
    int iter = 0, minIter = 30, restart = 0;
    int i, j, spId;
    double rValue, lossValue = 1e10, gradU, gradS;
    long double eij, wi, wj;
    vector<long double> eu(numUser, 1), es(numService, 1);
    srand(time(NULL));
    while(lossValue > convergeThreshold || iter < minIter) {
        // re-initialize U and S and restart iteration, if not converged
        // if (iter >= maxIter && restart < 10) {
        //     iter = 0;
        //     restart++;               
        //     for (int k = 0; k < dim; k++) {
        //         for (int a = 0; a < numUser; a++) {
        //             U[a][k] = ((double) rand()) / RAND_MAX;
        //         }
        //         for (int b = 0; b < numService; b++) {
        //             S[b][k] = ((double) rand()) / RAND_MAX;
        //         }
        //     }
        //     cout.setf(ios::fixed);            
        //     cout << currentDateTime() << ": ";
        //     cout << "re-initialize and restart..." << endl;                          
        // }
        
        // one iteration
        for (int s = 0; s < numSample; s++) {
            // random sampling
            spId = rand() % numSample;
            i = spIndex[spId].first;
            j = spIndex[spId].second;
            rValue = spValue[spId];

            // confidence updates
            long double uv = dotProduct(U[i], S[j], dim);
            double pValue = sigmoid(uv);
            eij = fabs(pValue - rValue) / rValue;
            wi = eu[i] / (eu[i] + es[j]);
            wj = es[j] / (eu[i] + es[j]);
            eu[i] = beta * wi * eij + (1 - beta * wi) * eu[i];
            es[j] = beta * wj * eij + (1 - beta * wj) * es[j];

            // gradient descent updates
            long double grad_sigmoid_uv = grad_sigmoid(uv);
            double sqr_rValue = sqr(rValue);
            for (int k = 0; k < dim; k++) {
                gradU = wi * (pValue - rValue) * grad_sigmoid_uv * S[j][k] / sqr_rValue
                    + lmda * U[i][k];
                gradS = wj * (pValue - rValue) * grad_sigmoid_uv * U[i][k] / sqr_rValue
                    + lmda * S[j][k];
                U[i][k] -= eta * gradU;
                S[j][k] -= eta * gradS;
            }
        }

        // update predMatrix and loss value
        getPredMatrix(false, removedMatrix, U, S, numUser, numService, dim, predMatrix);
        lossValue = loss(U, S, removedMatrix, predMatrix, lmda, numUser, numService, dim);
        lossValue = lossValue / numSample;

        // log the debug info to check convergence   
        if (debugMode) {
            cout.setf(ios::fixed);            
            cout << currentDateTime() << ": ";
            cout << "iter = " << iter << ", lossValue = " << lossValue << endl;
        }

        iter++;
    }

    // update predMatrix
    getPredMatrix(true, removedMatrix, U, S, numUser, numService, dim, predMatrix);

    delete2DMatrix(lastU);
    delete2DMatrix(lastS);
    delete ((char*) U);
    delete ((char*) S);
    delete ((char*) removedMatrix);
    delete ((char*) predMatrix);
}


double loss(double **U, double **S, double **removedMatrix, double **predMatrix, double lmda, 
    int numUser, int numService, int dim)
{
    int i, j, k;
    double loss = 0;

    // cost
    for (i = 0; i < numUser; i++) {
        for (j = 0; j < numService; j++) {
            if (fabs(removedMatrix[i][j]) > EPS) {
                loss += 0.5 * sqr((removedMatrix[i][j] - predMatrix[i][j]) / removedMatrix[i][j]);  
            }
        }
    }

    // L2 regularization
    for (k = 0; k < dim; k++) {
        for (i = 0; i < numUser; i++) {
            loss += 0.5 * lmda * sqr(U[i][k]);
        }
        for (j = 0; j < numService; j++) {
            loss += 0.5 * lmda * sqr(S[j][k]);
        }
    }

    return loss;
}


void getPredMatrix(bool flag, double **removedMatrix, double **U, double **S, int numUser, 
        int numService, int dim, double **predMatrix)
{
    int i, j;
    for (i = 0; i < numUser; i++) {
        for (j = 0; j < numService; j++) {
            if (flag == true || fabs(removedMatrix[i][j]) > EPS) {
                predMatrix[i][j] = sigmoid(dotProduct(U[i], S[j], dim));
            } 
        }
    }
}


double sigmoid(long double x)
{
    return 1 / (1 + exp(-x));
}


long double grad_sigmoid(long double x)
{
    return 1 / (2 + exp(-x) + exp(x));
}


long double dotProduct(double *vec1, double *vec2, int len)  
{
    long double product = 0;
    for (int i = 0; i < len; i++) {
        product += vec1[i] * vec2[i];
    }
    return product;
}


double **vector2Matrix(double *vector, int row, int col)  
{
    double **matrix = new double *[row];
    if (!matrix) {
        cout << "Memory allocation failed in vector2Matrix." << endl;
        return NULL;
    }

    for (int i = 0; i < row; i++) {
        matrix[i] = vector + i * col;  
    }
    return matrix;
}


double **createMatrix(int row, int col) 
{
    double **matrix = new double *[row];
    matrix[0] = new double[row * col];
    memset(matrix[0], 0, row * col * sizeof(double)); // Initialization
    int i;
    for (i = 1; i < row; i++) {
        matrix[i] = matrix[i - 1] + col;
    }
    return matrix;
}


void delete2DMatrix(double **ptr)
{
    delete ptr[0];
    delete ptr;
}


void copyMatrix(double **M1, double **M2, int row, int col)
{
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            M1[i][j] = M2[i][j];
        }
    }
}


const string currentDateTime() 
{
    time_t     now = time(0);
    struct tm  tstruct;
    char       buf[80];
    tstruct = *localtime(&now);
    // Visit http://en.cppreference.com/w/cpp/chrono/c/strftime
    // for more information about date/time format
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    return buf;
}


