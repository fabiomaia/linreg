#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "mb_interface.h"

#define OPCODE_RESET   4
#define OPCODE_STORE_X 0
#define OPCODE_STORE_Y 1
#define OPCODE_STORE_T 2
#define OPCODE_STORE_A 5
#define OPCODE_COMPUTE 3

#define CONSTANT_M 6
#define CONSTANT_N 2
#define SCALING_FACTOR 2048
#define CONSTANT_THRESHOLD 1

unsigned int reset()
{
	unsigned int inst = 0; // don't care
	inst = ((OPCODE_RESET << 29) | inst);

	xil_printf("\nInstruction 0x%08x: reset coprocessor registers and state", inst);
	putfsl(inst, 0);

	return inst;
}

unsigned int store_x(int x, unsigned int i, unsigned int j)
{
	unsigned int inst = (x & 0x007fffff);
	inst = ((j << 23) & 0x03800000) | inst;
	inst = ((i << 26) & 0x1C000000) | inst;
	inst = (OPCODE_STORE_X << 29) | inst;

	putfsl(inst, 0);
	xil_printf("\nInstruction 0x%08x: stored X[%d][%d] = 0x%08x = %d", inst, i, j, x, x);

	return inst;
}

unsigned int store_y(int y, unsigned int i)
{
	unsigned int inst = (y & 0x03ffffff);
	inst = ((i << 26) & 0x1C000000) | inst;
	inst = (OPCODE_STORE_Y << 29) | inst;

	putfsl(inst, 0);
	xil_printf("\nInstruction 0x%08x: stored Y[%d] = 0x%08x = %d", inst, i, y, y);

	return inst;
}

unsigned int store_t(int t, unsigned int i)
{
	unsigned int inst = (t & 0x03ffffff);
	inst = ((i << 26) & 0x1C000000) | inst;
	inst = (OPCODE_STORE_T << 29) | inst;

	putfsl(inst, 0);
	xil_printf("\nInstruction 0x%08x: stored T[%d] = 0x%08x = %d", inst, i, t, t);

	return inst;
}

unsigned int store_a(int a)
{
	unsigned int inst = (a & 0x03ffffff);
	inst = (OPCODE_STORE_A << 29) | inst;

	xil_printf("\nInstruction 0x%08x: stored alpha = 0x%08x = %d", inst, a, a);
	putfsl(inst, 0);

	return inst;
}

unsigned int compute(unsigned int iter)
{
	unsigned int inst = iter;
	inst = (OPCODE_COMPUTE << 29) | inst;

	putfsl(inst, 0);
	xil_printf("\nInstruction 0x%08x: issued iteration %d of gradient descent", inst, iter);

	return inst;
}

int abs(int i)
{
	return (i < 0 ? -i : i);
}

unsigned int has_converged(int Tnew[], int T[], int n)
{
	unsigned int flag = 1;

	for(unsigned int i = 0; i < n; i++) {
		if(abs(Tnew[i] - T[i]) > CONSTANT_THRESHOLD) {
			flag = 0;
		}
	}

	return flag;
}

void print_model(int T[], int n)
{
	xil_printf("\nModel is y = ");

	for (unsigned int i = 0; i < CONSTANT_N-1; i++) {
		xil_printf("%d*x^%d + ", T[i], i);
	}

	xil_printf("%d*x^%d", T[CONSTANT_N-1], CONSTANT_N-1);
}

int main()
{
	init_platform();

    int X[CONSTANT_M][CONSTANT_N] = {
    		{1*SCALING_FACTOR, 2.34*SCALING_FACTOR},
			{1*SCALING_FACTOR, 3.77*SCALING_FACTOR},
			{1*SCALING_FACTOR, 4.54*SCALING_FACTOR},
    		{1*SCALING_FACTOR, 5.81*SCALING_FACTOR},
    		{1*SCALING_FACTOR, 6.12*SCALING_FACTOR},
    		{1*SCALING_FACTOR, 5.01*SCALING_FACTOR}
    };

    int Y[CONSTANT_M] = {
    		4.12*SCALING_FACTOR,
			3.04*SCALING_FACTOR,
			3.19*SCALING_FACTOR,
			6.35*SCALING_FACTOR,
			4.73*SCALING_FACTOR,
			6.77*SCALING_FACTOR
    };

    int T[CONSTANT_N] = {
    		1.01*SCALING_FACTOR,
			2.02*SCALING_FACTOR
    };

    int alpha = 0.01*SCALING_FACTOR;

    // Reset coprocessor
    reset();

    // Store X matrix
    for (unsigned int i = 0; i < CONSTANT_M; i++) {
        for (unsigned int j = 0; j < CONSTANT_N; j++) {
        	store_x(X[i][j], i, j);
		}
    }

    // Store Y vector
	for (unsigned int i = 0; i < CONSTANT_M; i++) {
		store_y(Y[i], i);
	}

	// Store theta vector
	for (unsigned int i = 0; i < CONSTANT_N; i++) {
		store_t(T[i], i);
	}

	// Store learning rate value
	store_a(alpha);

	// Run gradient descent for linear regression until the algorithm converges
	int iter;
	for (iter = 1; iter < 1000; iter++) {
		// Issue new iteration of gradient descent
    	compute(iter);

        // Retrieve new theta vector
    	int Tnew[CONSTANT_N];
    	int Told[CONSTANT_N];
    	for (int l = 0; l < CONSTANT_N; l++) {
    		getfsl(Tnew[l], 0);
    		Told[l] = T[l];
    		T[l] = Tnew[l];
    	}

    	// Check if algorithm has converged
		if(has_converged(Tnew, Told, CONSTANT_N)) break;
	}

	reset();

	xil_printf("\n\nAlgorithm converged in %d iterations.", iter);
	print_model(T, CONSTANT_N);

    cleanup_platform();
    return 0;
}
