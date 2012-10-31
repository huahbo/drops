/****************************************
 *  Balakotaiah:                        \n*
 *   - instationary setup             \n*
 *   - constant diffusion             \n*
 *   - convection                     \n*
 *   - no reaction                    \n*
 ****************************************/
#include "poisson/poissonCoeff.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <string.h>
#include <sstream>
#include <math.h>
#include <gsl/gsl_spline.h>
#include <gsl/gsl_interp.h>
#include <gsl/gsl_errno.h>

extern DROPS::ParamCL P;

namespace Balakotaiah
{
  int getn(std::string dfile)
  {
    int n=0;
    double tmp;
    std::ifstream fd;
    fd.open(dfile.c_str());
    if(fd.fail())
      std::cout << dfile << " not available" << std::endl;

    while(!fd.eof()){
      if(fd.good()){
  fd >> tmp;
  n++;
      }
    }

    fd.close();
    return n;
  }

  int countTimesteps(const int starttime, const int stoptime, const int timeinc)
  {
    int tmp = 0;
    for(int i=starttime; i<=stoptime; i+=timeinc)
      tmp++;

    return tmp;
  }

  void getData(double *d, std::string dfile, std::string time)
  {
    std::ifstream fd;
    std::string file;
    file = dfile + time + ".dat";
    fd.open(file.c_str());
    int i=0;

    while(!fd.eof()){
      if(fd.good()){
  fd >> d[i];
  i++;
      }
    }

    fd.close();
  }

  static bool     init=false;

  static int      n;

  static gsl_spline ** hspline,
       ** qspline;

  static gsl_interp_accel * acc;

  static std::string hfile,
       qfile;

  static int  starttime,
       stoptime,
       timeinc;

  void initialize()
  {
    int id = omp_get_thread_num();
    //std::cout << "This is thread " << id << std::endl;
    if (id==0) { // only the master thread does this
      std::string file = P.get<std::string>("andreasOpt.DataDir");
      hfile = file + "/longh";
      qfile = file + "/longq";
      starttime = P.get<int>("andreasOpt.starttime");
      stoptime = P.get<int>("andreasOpt.stoptime");
      timeinc = P.get<int>("Time.StepSize");
      std::stringstream out;
      out << starttime;
      file=hfile+out.str()+".dat";
      n=getn(file);

      int tsteps = countTimesteps(starttime,stoptime,timeinc);
      acc = gsl_interp_accel_alloc();
      hspline = new gsl_spline*[tsteps];
      qspline = new gsl_spline*[tsteps];

      double * h,
  * q,
  * xa;

      h = new double[n];
      q = new double[n];
      xa = new double[n];

      for(int j=0; j<n; j++)
  xa[j] = j;

      std::cout << "Getting Data... ";
      for(int i=0; i<tsteps; i++){
  std::stringstream time;
  time << starttime + i*timeinc;
  getData(h, hfile, time.str());
  hspline[i] = gsl_spline_alloc(gsl_interp_cspline, n);
  gsl_spline_init(hspline[i], xa, h, n);
  getData(q, qfile, time.str());
  qspline[i] = gsl_spline_alloc(gsl_interp_cspline, n);
  gsl_spline_init(qspline[i], xa, q, n);
      }
      std::cout << " done" << std::endl;


      init=true;

      delete[] h;
      delete[] q;
      delete[] xa;
#pragma omp barrier
    }
  }

  double get_h(int t, double x)
  {
    if(!init)
      initialize();
    int nt = (int)((t+0.1)/timeinc);
    return gsl_spline_eval(hspline[nt], x, acc);
  }

  void uvh(double* res, int t, double x, double y)
  {
    double          hres[2],
      qres[2];
    if(!init)
      initialize();

    int nt = (int)((t+0.1)/timeinc);
    hres[0] = gsl_spline_eval(hspline[nt], x, acc);
    hres[1] = gsl_spline_eval_deriv(hspline[nt], x, acc);
    qres[0] = gsl_spline_eval(qspline[nt], x, acc);
    qres[1] = gsl_spline_eval_deriv(qspline[nt], x, acc);

    double h1 = hres[0];
    double h2 = h1*h1;
    double h3 = h2*h1;
    double h4 = h2*h2;
    double y2 = y*y;
    double y3 = y2*y;
    res[0]=3*qres[0]/h3*(hres[0]*y-y2/2);
    res[1]=-(1.5*qres[0]/h4*hres[1]-0.5*qres[1]/h3)*y3
      -(1.5*qres[1]/h2-3*qres[0]/h3*hres[1])*y2;
  }
  /*****************************************************************************************************************/

  double Interface( const DROPS::Point3DCL& p, double t)
  {
    //std::cout << "t: " << t  << "x: " << p[0] << "y: " << p[1] << std::endl;
    return get_h((int)(t+0.1), p[0]);
  }

  DROPS::Point3DCL Flowfield(const DROPS::Point3DCL& p, double t){
    double res[2];
    //std::cout << "t: " << t << std::endl;
    uvh(res,(int)(t+0.1),p[0],p[1]);
    DROPS::Point3DCL v(0.);
    v[0] = res[0];
    v[1] = res[1];
    v[2] = 0.;
    return v;
  }

  
  
}//end of namespace

class HQUV{

private:

double x;
int nt;

double h;
double h_x;
double h_xx;
double h_xxx;
double h_xxxx;

double q;
double q_x;
double q_xx;
double q_xxx;
double q_xxxx;

public:

HQUV(double X, int nT); //Konstruktor

double h0(void) {return h;}
double h1(void) {return h_x;}
double h2(void) {return h_xx;}
double h3(void) {return h_xxx;}
double h4(void) {return h_xxxx;}

double q0(void) {return q;}
double q1(void) {return q_x;}
double q2(void) {return q_xx;}
double q3(void) {return q_xxx;}
double q4(void) {return q_xxxx;}

double U1_0(void);
double U2_0(void);
double U1_1(void);
double U2_1(void);
double U1_2(void);
double U2_2(void);
double U1_3(void);
double U2_3(void);

double V2_0(void);
double V3_0(void);
double V2_1(void);
double V3_1(void);
double V2_2(void);
double V3_2(void);
double V2_3(void);
double V3_3(void);

};
///////////////////////Konstruktor definieren:
HQUV::HQUV(double X, int nT){
    x=X; nt=nT;

    h = gsl_spline_eval(hspline[nt], x, acc);
    h_x = gsl_spline_eval_deriv(hspline[nt], x, acc);
    h_xx;
    h_xxx;
    h_xxxx;

    q = gsl_spline_eval(qspline[nt], x, acc);
    q_x = gsl_spline_eval_deriv(qspline[nt], x, acc);
    q_xx;
    q_xxx;
    q_xxxx;


}
/////////////////////////////////////////////
double HQUV::U1_0(void){
    return 3.*q/(h*h);
}
double HQUV::U2_0(void){
    return (-1.5)*q/(h*h*h);
}
double HQUV::U1_1(void){
    return 3.*q_x/(h*h)-6.*(q*h_x)/(h*h*h);
}
double HQUV::U2_1(void){
    return -1.5*q_x/(h*h*h) + 4.5*(q*h_x)/(h*h*h*h);
}
double HQUV::U1_2(void){
    double a = 3.*q_xx/(h*h) - 12.*(q_x*h_x)/(h*h*h);
    double b = 18.*(q*h_x*h_x)/(h*h*h*h) - 6.*(q*h_xx)/(h*h*h);
    return a + b; 
}
double HQUV::U2_2(void){
    double a = -1.5*q_xx/(h*h*h) + 9.*(q_x*h_x)/(h*h*h*h);
    double b = -18.*(q*h_x*h_x)/(h*h*h*h*h) + 4.5*(q*h_xx)/(h*h*h*h);
    return a + b; 
}
double HQUV::U1_3(void){
    double a = 3.*q_xxx/(h*h) - 18.*(q_xx*h_x)/(h*h*h) + 54.*(q_x*h_x*h_x)/(h*h*h*h);
    double b = -18.*(q_x*h_xx)/(h*h*h) - 72.*(q*h_x*h_x*h_x)(h*h*h*h*h);
    double c = 54.*(q*h_x*h_xx)/(h*h*h*h) - 6.*(q*h_xxx)/(h*h*h); 
    return  a + b + c;
}
double HQUV::U2_3(void){
    double a = -1.5*q_xxx/(h*h*h) + 13.5*(q_xx*h_x)/(h*h*h*h) - 54.*(q_x*h_x*h_x)/(h*h*h*h*h);
    double b = 13.5*(q_x*h_xx)/(h*h*h*h) + 90.*(q*h_x*h_x*h_x)/(h*h*h*h*h*h);
    double c = 4.5*(q*h_xxx)/(h*h*h*h) - 54.*(q*h_x*h_xx)/(h*h*h*h*h);
    return a + b + c;
}
//////////////////////////////////////
double HQUV::V2_0(void){
    return 3.*(h_x*q)/(h*h*h) - 1.5*q_x/(h*h);
}
double HQUV::V3_0(void){
    return 0.5*q_x/(h*h*h) - 1.5*(h_x*q)/(h*h*h*h);
}
double HQUV::V2_1(void){
    return 6.*(q_x*h_x)/(h*h*h) - 9.*(q*h_x*h_x)/(h*h*h*h) + 3.*(q*h_xx)/(h*h*h) - 1.5*q_xx/(h*h);
}
double HQUV::V3_1(void){
    return 0.5*(q_xx)/(h*h*h) - 3.*(q_x*h_x)/(h*h*h*h) + 6.*(q*h_x*h_x)/(h*h*h*h*h) - 1.5*(q*h_xx)/(h*h*h*h);
}
double HQUV::V2_2(void){
    double a = 9.*(q_xx*h_x)/(h*h*h) - 27.*(q_x*h_x*h_x)/(h*h*h*h) + 9.*(q_x*h_xx)/(h*h*h);
    double b = 36.*(q*h_x*h_x*h_x)/(h*h*h*h*h) - 27.*(q*h_x*h_xx)/(h*h*h*h);
    double c = 3.*(q*h_xxx)/(h*h*h) - 1.5* q_xxx/(h*h);
    return a + b + c;
}
double HQUV::V3_2(void){
    double a = 0.5*(q_xxx)/(h*h*h) - 4.5*(q_xx*h_x)/(h*h*h*h) + 18.*(q_x*h_x*h_x)/(h*h*h*h*h);
    double b = -4.5*(q_x*h_xx)/(h*h*h*h) - 30.*(q*h_x*h_x*h_x)/(h*h*h*h*h*h);
    double c = 18.*(q*h_x*h_xx)/(h*h*h*h*h) -1.5.*(q*h_xxx)/(h*h*h*h);
    return a + b + c;
}
double HQUV::V2_3(void){
    double a = 12.*(q_xxx*h_xx)/(h*h*h) - 54.*(q_xx*h_x*h_x)/(h*h*h*h) + 18.*(q_xx*h_xx)/(h*h*h);
    double b = 144.*(q_x*h_x*h_x*h_x)/(h*h*h*h*h) - 108.*(q_x*h_x*h_xx)/(h*h*h*h);
    double c = 12.*(q_x*h_xxx)/(h*h*h) - 180.*(q*h_x*h_x*h_x*h_x)/(h*h*h*h*h*h);
    double d = 216.*(q*h_x*h_x*h_xx)/(h*h*h*h*h) - 27.*(q*h_xx*h_xx)/(h*h*h*h);
    double e = -36.*(q*h_x*h_xxx)/(h*h*h*h) + 3.*(q*h_xxxx)/(h*h*h) -1.5*(q_xxxx)/(h*h);
    return a + b + c + d + e;
}
double HQUV::V3_3(void){ 
    double a = 0.5*(q_xxxx)/(h*h*h) - 6.*(q_xxx*h_x)/(h*h*h*h) + 36.*(q_xx*h_x*h_x)/(h*h*h*h*h);
    double b = -9.*(q_xx*h_xx)/(h*h*h*h) - 120.*(q_x*h_x*h_x*h_x)/(h*h*h*h*h*h);
    double c = 72.*(q_x*h_x*h_xx)/(h*h*h*h*h) - 6.*(q_x*h_xxx)/(h*h*h*h);
    double d = 180.*(q*h_x*h_x*h_x*h_x)/(h*h*h*h*h*h*h) - 180.*(q*h_x*h_x*h_xx)/(h*h*h*h*h*h);
    double e = 18.*(q*h_xx*h_xx)/(h*h*h*h*h) + 24.*(q*h_x*h_xxx)/(h*h*h*h*h) - 1.5*(q*h_xxxx)/(h*h*h*h);
    return a + b + c + d + e;
}
/////////////////////////////////////////
double psurf_x(double h0, double h1, double h2, double h3, 
               double U1_0, double U1_1, double U1_2, 
               double U2_0, double U2_1, double U2_2, 
                            double V2_1, double V2_2,
                            double V3_1, double V3_2, double R, double W){
    double a = (((-8.)*h1)/(1+h1*h1))*(U1_1 + 2.*U2_1*h0 + 2.*U2_0*h1 + V2_2*h0*h0 + 2.*V2_1*h0*h1 + V3_2*h0*h0*h0 + 3.*V3_1*h0*h0*h1);
    double b = (((-8.)*h2)/(1 + h1*h1))*(U1_0 + 2.*U2_0*h0 + V2_1*h0*h0 + V3_1*h0*h0*h0);
    double c = ((16.*h1*h1*h2)/((1+h1*h1)*(1+h1*h1)))*(U1_0 + 2.*U2_0*h0 + V2_1*h0*h0 + V3_1*h0*h0*h0);
    double d = ((-8.)*(1-h1*h1)/(1+h1*h1))*(U1_2*h0 + U1_1*h1 + U2_2*h0*h0 + 2.*U2_1*h0*h1);
    double e = (16.*(h1*h2)/(1+h1*h1))*(U1_1*h0 + U2_1*h0*h0);
    double f = (16.*((1-h1*h1)*h1*h2)/((1+h1*h1)*(1+h1*h1)))*(U1_1*h0 + U2_1*h0*h0);
    double g = R*W*((3.*h2*h2*h1)/((1+h1*h1)*(1+h1*h1)*sqrt(1+h1*h1))  +   (h3)/((1+h1*h1)*sqrt(1+h1*h1)));
               
    return a + b + c + d + e  + f + g;         
               
}

double diff_t(double A_t, double A_tplusdt, double dt){
    return (A_tplusdt - A_t)/dt;
}

double pbulk_x(double U1_0, double U1_1, 
               double U2_0, double U2_1, 
               double V2_0, double V2_1, double V2_2, double V2_3, double dtV2_0, double dtV2_1,
               double V3_0, double V3_1, double V3_2, double V2_3, double dtV3_0, double dtV3_1,
               double h0, double h1, double y, double R, double deltat){
    double a = y*8.*V2_1 + y*y*12.*V3_1 + y*y*y*(1./3.)*(4.*V2_3 - R*diff_t(V2_1,dtV2_1,deltat));
    double b = y*y*y*y*(V3_3 - 0.25*R*diff_t(V3_1,dtV3_1,deltat) - R*V2_0*V2_1 - 0.25*R*U1_0*V2_2 - 0.25*R*U1_1*V2_1);
    double c = (-0.2)*R*y*y*y*y*y*(5.*V2_0*V3_1 + 5.*V2_1*V3_0 + U1_1*V3_1 + U1_0*V3_2 + U2_1*V2_1 + U2_0*V2_2);
    double e = (-1./6.)*R*y*y*y*y*y*y*(U2_0*V3_2 + U2_1*V3_1 + 6.*V3_0*V3_1);
    double f = (1./3.)*R*diff_t(V2_1,dtV2_1,deltat) + (1./4.)*R*diff_t(V3_1,dtV3_1,deltat);              
    double g = 5.*R*V2_0*V3_0*h0*h0*h0*h0*h1 - 4.*V2_2*h0*h0*h1 - 4.*V3_2*h0*h0*h0*h1 + (1./6.)*R*U2_0*V3_2*h0*h0*h0*h0*h0*h0;
    double h = 0.2*R*U1_0*V3_2*h0*h0*h0*h0*h0 + 0.2*R*U2_0*V2_2*h0*h0*h0*h0*h0;
    double i = 0.25*R*U1_0*V2_2*h0*h0*h0*h0 - V3_3*h0*h0*h0*h0 - (4./3.)*V2_3*h0*h0*h0;
    
    return a + b + c + d + e + f + g +h +i;          
}
                              
int main() {
const double Re;
const double We;
const double cotbeta;
const double delta_t;
const double y;
const double x=5.;
const int n1 = 30;

const int n2 = n1 + 1;

HQUV t(x,n1);
HQUV t_dt(x,n2);







return 0;
}
