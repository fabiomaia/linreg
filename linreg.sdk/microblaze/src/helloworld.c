#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "mb_interface.h"
#include "xparameters.h"
#include "xintc_l.h"
#include "xil_exception.h"
#include "xstatus.h"
#include <stdint.h>

#include "instructions.h"
#include "linreg.h"

#define INTC_BASEADDR			XPAR_INTC_0_BASEADDR
#define INTC_DEVICE_ID			XPAR_INTC_0_DEVICE_ID
#define FIT_TIMER_0_INT_ID		XPAR_AXI_INTC_0_FIT_TIMER_0_INTERRUPT_INTR
#define FIT_TIMER_0_INT_MASK	XPAR_FIT_TIMER_0_INTERRUPT_MASK

#define CONSTANT_M 6
#define CONSTANT_N 2
#define SCALING_FACTOR 2048
#define CONSTANT_THRESHOLD 1

static int irqCount = 0;

void tic()
{
	irqCount = 0;
}

void print_float(float Input)
{
    /*
     * cast input and remove floating part
     */

    long int fix_part = (long int) Input;
    /*
     * remove integer part, multiply by 1000 to adjust to 3 decimal points then cast to integer
     */
    long int frac_part = (long int) (Input*1000.0 - fix_part*1000);

    xil_printf("%d", fix_part);
    xil_printf(".%d\r\n", frac_part);
}

// Copyright (c) 2018 Ricardo J. Jesus
char *print_long_long(long long x)
{
	static char buf[21];
	buf[20] = '\0';
	int i = 19;

	while(x) {
		buf[i--] = '0' + (x % 10);
		x /= 10;
	}

	return &buf[i+1];
}

void toc()
{
	int period = 50; // one clock cycle in nanoseconds
	int cycles = 100000; // an event is generated every 100000 cycles
	long long res = ((long long)period)*irqCount*cycles;
	xil_printf("\nElapsed time is estimated to be %s ns (%d interrupts)", print_long_long(res), irqCount);
}

// Will be called at every timer output event
void TimerIntCallbackHandler(void *CallbackRef)
{
	irqCount++;
}

int SetupInterrupts(u32 IntcBaseAddress)
{
	/*
	 * Connect a callback handler that will be called when an interrupt for the timer occurs,
	 * to perform the specific interrupt processing for the timer.
	 */
	XIntc_RegisterHandler(IntcBaseAddress, FIT_TIMER_0_INT_ID, (XInterruptHandler)TimerIntCallbackHandler, (void *)0);

	/*
	 * Enable interrupts for all devices that cause interrupts, and enable
	 * the INTC master enable bit.
	 */
	XIntc_EnableIntr(IntcBaseAddress, FIT_TIMER_0_INT_MASK);

	/*
	 * Set the master enable bit.
	 */
	XIntc_Out32(IntcBaseAddress + XIN_MER_OFFSET, XIN_INT_HARDWARE_ENABLE_MASK | XIN_INT_MASTER_ENABLE_MASK);

	/*
	 * This step is processor specific, connect the handler for the
	 * interrupt controller to the interrupt source for the processor.
	 */
	Xil_ExceptionInit();

	/*
	 * Register the interrupt controller handler with the exception table.
	 */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT, (Xil_ExceptionHandler)XIntc_DeviceInterruptHandler, INTC_DEVICE_ID);

	/*
	 * Enable exceptions.
	 */
	Xil_ExceptionEnable();

	return XST_SUCCESS;
}

int main()
{
	init_platform();

	int status = SetupInterrupts(INTC_BASEADDR);
	if (status != XST_SUCCESS) {
		xil_printf("Setup interrupts failed\r\n");
		cleanup_platform();
		return XST_FAILURE;
	}
	xil_printf("Setup interrupts successful\r\n");

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
    tic();
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
		if(has_converged(Tnew, Told, CONSTANT_N, CONSTANT_THRESHOLD)) break;
	}

	reset();
	toc();

	xil_printf("\n\nAlgorithm converged in %d iterations.", iter);
	print_model(T, CONSTANT_N);

    cleanup_platform();
    return 0;
}
