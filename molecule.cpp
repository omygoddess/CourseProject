#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <stdio.h>
#include <string>


#include "molecule.h"



using namespace std;

void Molecule::SetParameters() {

	num_atoms = 1 + ns * (pow(2, num_generation + 1) - 1);
	theta = sigma * num_atoms;
};


void Molecule::AllocateMemory(int layers_x_, int layers_y_, int M) {

	layers_x = layers_x_;
	layers_y = layers_y_;
	Gback.assign(layers_x + 2, vector <vector<double>>(layers_y + 2, vector<double>(num_atoms + 2, 0)));
	Gforw.assign(layers_x + 2, vector <vector<double>>(layers_y + 2, vector<double>(num_atoms + 2, 0)));;
	u.resize(M+2,0);
	G.assign(layers_x + 2, vector<double>(layers_y + 2, 0));
	fi_side.resize(M+2,0);
	fi_p.assign(layers_x + 2, vector<double>(layers_y + 2, 0));
	multipliers.assign(layers_x + 2, vector <double>(layers_y + 2, 0));
};

void Molecule::FindG() {
	for (int i = 0; i < layers_x + 1; ++i) {
		for (int j = 0; j < layers_y + 1; ++j) {
			G[i][j] = exp(-u[i*(layers_y + 2) + j]);
		}
	}

};

void Molecule::FindGforw(Geometry geo) {
	for (int j = 1; j < num_atoms; ++j) {
		for (int i = 1; i < layers_x + 1; ++i) {
			for (int k = 1; k < layers_y + 1; ++k) {

				Gforw[i][k][j] = G[i][k] * (geo.lambda_bb[i][k] * Gforw[i - 1][k - 1][j - 1] + geo.lambda_bn[i][k] * Gforw[i - 1][k][j - 1] + geo.lambda_bf[i][k] * Gforw[i - 1][k + 1][j - 1] +
					geo.lambda_nb[i][k] * Gforw[i][k - 1][j - 1] + geo.lambda_nn[i][k] * Gforw[i][k][j - 1] + geo.lambda_nf[i][k] * Gforw[i][k + 1][j - 1] +
					geo.lambda_fb[i][k] * Gforw[i + 1][k - 1][j - 1] + geo.lambda_fn[i][k] * Gforw[i + 1][k + 1][j - 1] + geo.lambda_ff[i][k] * Gforw[i + 1][k + 1][j - 1]);
				//периодические граничные условия
				Gforw[layers_x + 1][k][j] = Gforw[layers_x][k][j];
				Gforw[0][k][j] = 0;
			}
                     //зеркальные граничные условия + стенка 
			Gforw[i][layers_y + 1][j] = Gforw[i][1][j];
			Gforw[i][0][j] = Gforw[i][layers_y][j];
		}
	}
};


void Molecule::FindGback(Geometry geo) {

	for (int j = num_atoms - 1; j > 0; --j) {
		for (int i = 1; i < layers_x + 1; ++i) {
			for (int k = 1; k < layers_y + 1; ++k) {
				Gback[i][k][j - 1] = G[i][k] * (geo.lambda_bb[i][k] * Gback[i - 1][k - 1][j] + geo.lambda_bn[i][k] * Gback[i - 1][k][j] + geo.lambda_bf[i][k] * Gback[i - 1][k + 1][j] +
					geo.lambda_nb[i][k] * Gback[i][k - 1][j] + geo.lambda_nn[i][k] * Gback[i][k][j] + geo.lambda_nf[i][k] * Gback[i][k + 1][j] +
					geo.lambda_fb[i][k] * Gback[i + 1][k - 1][j] + geo.lambda_fn[i][k] * Gback[i + 1][k][j] + geo.lambda_ff[i][k] * Gback[i + 1][k + 1][j]);

				//периодические граничные условия
				Gback[layers_x + 1][k][j - 1] = Gback[layers_x][k][j - 1];
				Gback[0][k][j - 1] = 0;
			}

			//зеркальные граничные условия + стенка
			Gback[i][layers_y + 1][j - 1] = Gback[i][1][j - 1];
			Gback[i][0][j - 1] = Gback[i][layers_y][j - 1];
		}
	}
};



void Molecule::FindQ(Geometry geo) {

	double sum = 0;
	q = 0;
	for (int i = 1; i < layers_x + 1; ++i) {
		for (int k = 1; k < layers_y + 1; ++k) {
			sum = 0;
			for (int j = 0; j < num_atoms; ++j)
				sum += (Gforw[i][k][j] * Gback[i][k][j]) / G[i][k];
			q += sum * geo.volume[i][k];
		}
	}
};

void Molecule::FindFiP() {
	double sum;
	for (int i = 1; i < layers_x + 1; ++i) {
		for (int k = 1; k < layers_y + 1; ++k) {
			sum = 0;
			for (int j = 0; j < num_atoms; ++j)
				sum += (Gforw[i][k][j] * Gback[i][k][j]) / G[i][k];
			fi_p[i][k] = (theta / q)*sum;
		}
	}
	//периодические граничные условия
	for (int i = 1; i < layers_x + 1; ++i) {
		fi_p[i][layers_y + 1] = fi_p[i][1];
		fi_p[i][0] = fi_p[i][layers_y];
	}
	//зеркальные граничные условия + стенка
	for (int k = 1; k < layers_y + 1; ++k) {
		fi_p[layers_x + 1][k] = fi_p[layers_x][k];
		fi_p[0][k] = 0;
	}
};


void Molecule::FindFiSide(Geometry geo) {

	for (int l = 1; l < layers_x + 1; ++l) {
		for (int k = 1; k < layers_y + 1; ++k) {
			fi_side[l * (layers_y + 2) + k] = 0.0;
		}
	}

	for (int i = -1; i <= 1; ++i) {
		for (int j = -1; j <= 1; ++j) {
			if (i == j == -1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_bb[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == -1 && j == 0) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_bn[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == -1 && j == 1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_bf[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 0 && j == -1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_nb[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 0 && j == 0) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_nn[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 0 && j == 1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_nf[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 1 && j == -1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_fb[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 1 && j == 0) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_fn[l][k] * fi_p[l + i][k + j];
				}
			}
			else if (i == 1 && j == 1) {
				for (int l = 1; l < layers_x + 1; ++l) {
					for (int k = 1; k < layers_y + 1; ++k)
						fi_side[l * (layers_y + 2) + k] += geo.lambda_ff[l][k] * fi_p[l + i][k + j];
				}
			}
		}
	}
}
