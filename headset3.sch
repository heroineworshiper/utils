v 20130925 2
C 48800 47600 1 180 1 lm324-1.sym
{
T 49625 47450 5 8 0 0 180 6 1
device=LM324
T 49000 46700 5 10 1 1 180 6 1
refdes=U?
}
C 52000 47400 1 180 1 lm324-1.sym
{
T 52825 47250 5 8 0 0 180 6 1
device=LM324
T 52200 46500 5 10 1 1 180 6 1
refdes=U?
}
C 56200 48700 1 180 1 lm324-1.sym
{
T 57025 48550 5 8 0 0 180 6 1
device=LM324
T 56400 47800 5 10 1 1 180 6 1
refdes=U?
}
C 52000 49400 1 180 1 lm324-1.sym
{
T 52825 49250 5 8 0 0 180 6 1
device=LM324
T 52200 48500 5 10 1 1 180 6 1
refdes=U?
}
C 47900 47600 1 180 0 capacitor-2.sym
{
T 47700 46900 5 10 0 0 180 0 1
device=POLARIZED_CAPACITOR
T 48000 47100 5 10 1 1 180 0 1
refdes=2.2uF 16V
T 47700 46700 5 10 0 0 180 0 1
symversion=0.1
T 47200 46800 5 10 1 1 0 0 1
netname=THE C
}
C 58900 48100 1 0 0 capacitor-2.sym
{
T 59100 48800 5 10 0 0 0 0 1
device=POLARIZED_CAPACITOR
T 59100 48600 5 10 1 1 0 0 1
refdes=100uF 16V
T 59100 49000 5 10 0 0 0 0 1
symversion=0.1
}
C 51100 50400 1 180 0 capacitor-2.sym
{
T 50900 49700 5 10 0 0 180 0 1
device=POLARIZED_CAPACITOR
T 50900 49900 5 10 1 1 180 0 1
refdes=2.2uF 16V
T 50900 49500 5 10 0 0 180 0 1
symversion=0.1
}
C 47200 48300 1 90 0 capacitor-2.sym
{
T 46500 48500 5 10 0 0 90 0 1
device=POLARIZED_CAPACITOR
T 46700 48500 5 10 1 1 90 0 1
refdes=100uF 10V
T 46300 48500 5 10 0 0 90 0 1
symversion=0.1
}
C 48100 44500 1 270 0 capacitor-2.sym
{
T 48800 44300 5 10 0 0 270 0 1
device=POLARIZED_CAPACITOR
T 48600 44300 5 10 1 1 270 0 1
refdes=100uF 10V
T 49000 44300 5 10 0 0 270 0 1
symversion=0.1
}
C 47900 47300 1 0 0 resistor-1.sym
{
T 48200 47700 5 10 0 0 0 0 1
device=RESISTOR
T 48100 47600 5 10 1 1 0 0 1
refdes=10k
}
C 48900 48000 1 0 0 resistor-1.sym
{
T 49200 48400 5 10 0 0 0 0 1
device=RESISTOR
T 49100 48300 5 10 1 1 0 0 1
refdes=100k
}
C 46900 48300 1 270 0 resistor-1.sym
{
T 47300 48000 5 10 0 0 270 0 1
device=RESISTOR
T 47200 48100 5 10 1 1 270 0 1
refdes=10k
}
C 51100 50100 1 0 0 resistor-1.sym
{
T 51400 50500 5 10 0 0 0 0 1
device=RESISTOR
T 51300 50400 5 10 1 1 0 0 1
refdes=10k
}
C 54600 46900 1 0 0 resistor-1.sym
{
T 54900 47300 5 10 0 0 0 0 1
device=RESISTOR
T 54800 47200 5 10 1 1 0 0 1
refdes=10k
}
C 53500 47900 1 180 0 resistor-variable-1.sym
{
T 52700 47000 5 10 0 0 180 0 1
device=VARIABLE_RESISTOR
T 53200 48100 5 10 1 1 180 0 1
refdes=100k
}
C 53500 50300 1 180 0 resistor-variable-1.sym
{
T 52700 49400 5 10 0 0 180 0 1
device=VARIABLE_RESISTOR
T 53200 50500 5 10 1 1 180 0 1
refdes=100k
}
N 48800 47400 48800 48100 4
N 48800 48100 48900 48100 4
N 49800 47200 50400 47200 4
N 49800 48100 50100 48100 4
N 50100 48100 50100 47200 4
N 52600 50200 52000 50200 4
N 53000 49700 53000 49000 4
N 47500 44500 56200 44500 4
N 52000 46800 52000 44500 4
N 48800 47000 48800 44500 4
N 56200 48100 56200 44500 4
T 49700 51400 9 10 1 0 0 0 1
PHONE SPEAKER OUT
N 50200 50200 50200 51300 4
N 47000 47400 47000 46400 4
N 47000 46400 46700 46400 4
T 41900 47500 9 10 1 0 0 0 2
PHONE MIC IN

T 42800 46500 9 10 1 0 0 0 1
HEADSET MIC OUT
C 47300 49500 1 0 0 5V-plus-1.sym
C 48200 43300 1 0 0 gnd-1.sym
C 50400 47100 1 0 0 resistor-1.sym
{
T 50700 47500 5 10 0 0 0 0 1
device=RESISTOR
T 50600 47400 5 10 1 1 0 0 1
refdes=10k
}
N 59800 48300 60300 48300 4
T 58300 47800 9 10 1 0 0 0 1
HEADSET SPEAKER IN
T 51800 48000 9 10 1 0 0 0 1
MIC GAIN
T 52300 50600 9 10 1 0 0 0 1
SPEAKER GAIN
T 49000 46200 9 10 1 0 0 0 1
THE PREAMP
T 52100 45900 9 10 1 0 0 0 2
MIC GAIN

T 56300 47400 9 10 1 0 0 0 1
SPEAKER MIXER
T 47800 44800 9 10 1 0 0 0 1
VIRTUAL GROUND
T 47200 48300 9 10 1 0 0 0 1
PLUG-IN POWER
C 46500 48000 1 180 0 lm324-1.sym
{
T 45675 47850 5 8 0 0 180 0 1
device=LM324
T 46300 47100 5 10 1 1 180 0 1
refdes=U?
}
N 47000 47400 46500 47400 4
N 46500 47800 46500 48300 4
N 46500 48300 45500 48300 4
N 45500 48300 45500 47600 4
N 42400 47600 43500 47600 4
T 42000 47100 9 10 1 0 0 0 2
ROUTE AWAY FROM 
HEADSET MIC
C 45800 46300 1 0 0 inductor-1.sym
{
T 46000 46800 5 10 0 0 0 0 1
device=INDUCTOR
T 46000 46600 5 10 1 1 0 0 1
refdes=L?
T 46000 47000 5 10 0 0 0 0 1
symversion=0.1
}
N 45800 46400 43000 46400 4
T 45900 46200 9 10 1 0 0 0 1
RF CHOKE
N 47500 49500 47500 48300 4
N 47500 48300 47000 48300 4
C 47100 49800 1 180 0 gnd-1.sym
N 47000 49500 47000 49200 4
C 44400 47800 1 180 0 capacitor-2.sym
{
T 44200 47100 5 10 0 0 180 0 1
device=POLARIZED_CAPACITOR
T 44500 47300 5 10 1 1 180 0 1
refdes=2.2uF 16V
T 44200 46900 5 10 0 0 180 0 1
symversion=0.1
}
C 45100 42500 1 0 0 npn-2.sym
{
T 45700 43000 5 10 0 0 0 0 1
device=NPN_TRANSISTOR
}
C 43700 43000 1 270 0 capacitor-2.sym
{
T 44400 42800 5 10 0 0 270 0 1
device=POLARIZED_CAPACITOR
T 44200 42800 5 10 1 1 270 0 1
refdes=10uF 10V
T 44600 42800 5 10 0 0 270 0 1
symversion=0.1
}
C 43800 44900 1 90 0 resistor-1.sym
{
T 43400 45200 5 10 0 0 90 0 1
device=RESISTOR
T 43500 45100 5 10 1 1 90 0 1
refdes=100k
}
N 45600 42500 45600 41800 4
N 43900 42100 43900 41600 4
N 45600 43500 45600 46400 4
C 43800 41300 1 0 0 gnd-1.sym
C 45500 41500 1 0 0 gnd-1.sym
C 48400 48900 1 0 0 led-2.sym
{
T 48700 49300 5 10 1 1 0 0 1
refdes=POWER
T 48500 49500 5 10 0 0 0 0 1
device=LED
}
C 47500 48900 1 0 0 resistor-1.sym
{
T 47800 49300 5 10 0 0 0 0 1
device=RESISTOR
T 47700 49200 5 10 1 1 0 0 1
refdes=500R
}
C 49600 48900 1 90 0 gnd-1.sym
C 43500 45900 1 0 0 5V-plus-1.sym
C 56200 49500 1 0 0 resistor-1.sym
{
T 56500 49900 5 10 0 0 0 0 1
device=RESISTOR
T 56400 49800 5 10 1 1 0 0 1
refdes=20k
}
N 57100 49600 57200 49600 4
N 57200 49600 57200 48300 4
N 56200 49600 56200 48500 4
N 52000 48300 52000 48800 4
N 52000 48300 54200 48300 4
N 54200 48300 54200 44500 4
N 52000 49200 52000 50200 4
N 53000 49000 54600 49000 4
N 53000 47000 54600 47000 4
C 54600 48900 1 0 0 resistor-1.sym
{
T 54900 49300 5 10 0 0 0 0 1
device=RESISTOR
T 54800 49200 5 10 1 1 0 0 1
refdes=10k
}
N 55500 49000 56200 49000 4
N 55500 47000 55800 47000 4
N 55800 47000 55800 49000 4
N 57200 48300 57800 48300 4
C 44100 43900 1 90 0 switch-spdt-1.sym
{
T 43300 44300 5 10 0 0 90 0 1
device=SPDT
T 43500 44200 5 10 1 1 90 0 1
refdes=MUTE
}
C 44200 46200 1 180 0 gnd-1.sym
C 44200 45000 1 90 0 resistor-1.sym
{
T 43800 45300 5 10 0 0 90 0 1
device=RESISTOR
T 43900 45200 5 10 1 1 90 0 1
refdes=22k
}
N 44100 44800 44100 45000 4
N 52000 47200 51300 47200 4
N 52600 47800 51800 47800 4
N 51800 47800 51800 47200 4
N 53000 47300 53000 47000 4
N 43900 43900 43900 43000 4
N 43700 45800 43700 45900 4
N 43700 44900 43700 44800 4
C 57800 48200 1 0 0 resistor-1.sym
{
T 58100 48600 5 10 0 0 0 0 1
device=RESISTOR
T 58000 48500 5 10 1 1 0 0 1
refdes=50R
}
N 58700 48300 58900 48300 4
C 47300 45600 1 0 0 5V-plus-1.sym
C 44600 47500 1 0 0 resistor-1.sym
{
T 44900 47900 5 10 0 0 0 0 1
device=RESISTOR
T 44800 47800 5 10 1 1 0 0 1
refdes=50R
}
N 44600 47600 44400 47600 4
C 47600 44500 1 90 0 resistor-1.sym
{
T 47200 44800 5 10 0 0 90 0 1
device=RESISTOR
T 47300 44800 5 10 1 1 90 0 1
refdes=10k
}
N 47500 45600 47500 45400 4
T 44100 43500 9 10 1 0 0 0 1
POP SOFTENER
C 46300 41900 1 0 0 npn-2.sym
{
T 46900 42400 5 10 0 0 0 0 1
device=NPN_TRANSISTOR
}
T 45700 44200 9 10 1 0 0 0 1
MUTE LED
C 46700 43900 1 270 0 resistor-1.sym
{
T 47100 43600 5 10 0 0 270 0 1
device=RESISTOR
T 46500 43500 5 10 1 1 270 0 1
refdes=500R
}
C 46700 41500 1 0 0 gnd-1.sym
N 46800 42900 46800 43000 4
C 46700 44900 1 270 0 led-2.sym
{
T 47000 44200 5 10 1 1 270 0 1
refdes=POWER
T 47300 44800 5 10 0 0 270 0 1
device=LED
}
N 46800 44000 46800 43900 4
C 46600 45100 1 0 0 5V-plus-1.sym
N 46800 45100 46800 44900 4
C 46200 42500 1 90 0 resistor-1.sym
{
T 45800 42800 5 10 0 0 90 0 1
device=RESISTOR
T 45900 42700 5 10 1 1 90 0 1
refdes=10k
}
C 44200 42900 1 0 0 resistor-1.sym
{
T 44500 43300 5 10 0 0 0 0 1
device=RESISTOR
T 44400 43200 5 10 1 1 0 0 1
refdes=10k
}
N 44200 43000 43900 43000 4
N 43900 43900 46100 43900 4
N 46100 43900 46100 43400 4
N 46100 42500 46100 42400 4
N 46100 42400 46300 42400 4
N 46800 41900 46800 41800 4
N 43500 47600 43500 48400 4
C 43600 48400 1 90 0 resistor-1.sym
{
T 43200 48700 5 10 0 0 90 0 1
device=RESISTOR
T 43300 48700 5 10 1 1 90 0 1
refdes=5k
}
C 43600 49600 1 180 0 gnd-1.sym
T 43600 48700 9 10 1 0 0 0 1
PHONE MIC DETECT
