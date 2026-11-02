// Logic Quest Bot : Task 1A : PWM Generator
/*
Instructions
-------------------
Students are not allowed to make any changes in the Module declaration.
This file is used to design a module which will scale down the clk_5MHz Clock Frequency to 500Hz and perform Pulse Width Modulation on it.

Recommended Quartus Version : 20.1
The submitted project file must be 20.1 compatible as the evaluation will be done on Quartus Prime Lite 20.1.

Warning: The error due to compatibility will not be entertained.
-------------------
*/

//PWM Generator
//Inputs : clk_5MHz, pulse_width
//Output : clk_500Hz, pwm_signal

module pwm_generator(
    input clk_5MHz,
    input reset_n,
    input [4:0] pulse_width,
    output reg clk_500Hz, pwm_signal
);

//////////////////DO NOT MAKE ANY CHANGES ABOVE THIS LINE //////////////////

/*
 add your code here 
 */
 
//////////////////DO NOT MAKE ANY CHANGES BELOW THIS LINE//////////////////

endmodule

