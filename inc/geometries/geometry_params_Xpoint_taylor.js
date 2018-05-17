/* Input-File for COMPASS axisymmetric taylor state equilibrium *
 * Te_50eV,B0_1T,deuterium, Cref~2e-5
 * X-point at
 *(R_0-1.1*triangularity*inverseaspectratio,-1.1*elongation*inverseaspectratio)
 *          ----------------------------------------------------------- */
{
    "equilibrium" : "taylor", //taylor state
    //----------------------Taylor coefficients---------------
    "c" :[-0.730004529864617913443309878090712685057289011596696572774289301646,
    -0.013752279867251353918502943858642437963550235876487492144645446786828705,
    -1.043934755061476893780514356717827737689350282272451493765507415754555292,
    0.0652044051606185896009860257256201123783699931775930983650947832207138720,
    -0.075588684108624986707612696927980524120446412329125959454845935222288227,
    -0.166244745975333508158120968358969752878367824897146548390838214529077855,
    0.2339447574961647090247023551091674910845416152319523071184183514964211299,
    -0.106340609874360718259386870009150862837873566733690651435539105985033601,
    -0.195348950338876734547335892506041028973952380901869144068532010742858264,
    0.0185335490893965223839308241407808534672715395435233413969584688870300260,
    3.4373031197519723201231633783929522278128352814841152895441705480656431803,
    4.9016827404530536864875893866884491045861716739824591650253786527604617409],
    //----------------------geometric coefficients-------------
    "R_0"                : 547.891714877869, //(in units of rhos)
    "inverseaspectratio" : 0.41071428571428575, //a/R_0
    "elongation"         : 1.75,
    "triangularity"      : 0.25,
    //----------------------Miscellaneous----------------------
    "alpha"         :  0.05, //damping thickness
    "rk4eps"        :  1e-4,
    "psip_min"      : -100,
    "psip_max"      :  0.0, // (> -7.4)
    "psip_max_cut"  :  1e10,
    "psip_max_lim"  :  1e10,
    "qampl"         :  1
}
//@ ----------------------------------------------------------



