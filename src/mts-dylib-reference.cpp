/*
 * An implementation of the MTS-ESP middleware dylib.
 *
 * For more information, see oddsound.com
 *
 * Released under the MIT license
 */

#include <iostream>
#include <cstring>
#include <cstdint>
#include <cmath>

#if IPC_SUPPORT
#include <sys/shm.h>
#endif

#define LOGDAT std::cout << __func__ << " " << __FILE__ << ":" << __LINE__ << " | "
#define LOGFN LOGDAT << std::endl;

static bool tuningInitialized = false;
static void setDefaultTuning(double *t)
{
   for (int i=0;i<128;i++) t[i]=440.*pow(2.,(i-69.)/12.);
}

static constexpr size_t memSize{sizeof(bool) + sizeof(int32_t) + 128 * sizeof(double)};
bool *hasMaster{nullptr};
int32_t *numClients{nullptr};
double *tuning{nullptr};

#if !IPC_SUPPORT
uint8_t memory[memSize];
#endif

bool connectToMemory()
{
#if IPC_SUPPORT
   // TODO: This really needs some error checking
   key_t key = ftok("mtsesp", 65);
   int shmid = shmget(key, memSize, 0666 | IPC_CREAT);
   if (shmid < 0)
   {
      LOGDAT << "Unable to create shared memory segment" << std::endl;
      return false;
   }
   hasMaster = (bool*)shmat(shmid, (void*)0, 0);
   numClients = (int32_t*) hasMaster + sizeof(bool);
   tuning = (double*) numClients + sizeof(int32_t);
#else
   hasMaster = (bool *)(&memory[0]);
   numClients = (int32_t*) hasMaster + sizeof(bool);
   tuning = (double*) numClients + sizeof(int32_t);
#endif
   return true;
}

extern "C"
{

   // Master-side API
   void MTS_RegisterMaster(void *) {
      LOGFN;
      connectToMemory();
      *hasMaster = true;
      *numClients = 0;
      if (!tuningInitialized)
      {
         setDefaultTuning(tuning);
         tuningInitialized = true;
      }
   }
   void MTS_DeregisterMaster() {
      LOGFN;
      *hasMaster = false;
      *numClients = 0;
   }
   bool MTS_HasMaster() {
      LOGDAT << *hasMaster << std::endl;
      return *hasMaster;
   }
   bool MTS_HasIPC() {
      LOGFN;
#if IPC_SUPPORT
      return true;
#else
      return false;
#endif
   }

   void MTS_Reinitialize() {
      LOGFN;
      *hasMaster = false;
      *numClients = 0;
      setDefaultTuning(tuning);
   }

   int MTS_GetNumClients() {
      return *numClients;
   }

   void MTS_SetNoteTunings(const double *d)
   {
      LOGFN;
      for (int i=0; i<128; ++i)
         tuning[i] = d[i];
   }

   void MTS_SetNoteTuning(double f, char idx)
   {
      LOGDAT << f << " " << (int)idx << std::endl;
      tuning[idx] = f;
   }

   char scaleName[256]{0};
   void MTS_SetScaleName(const char *s)
   {
      LOGDAT << s << std::endl;
      strncpy(scaleName, s, 255);
   }

   // Don't implement note filtering or channel specific tuning yet
   void MTS_FilterNote(bool, char , char ){}
   void MTS_ClearNoteFilter() {}
   void MTS_SetMultiChannel(bool, char) {}
   void MTS_SetMultiChannelNoteTunings(const double *, char) {}
   void MTS_SetMultiChannelNoteTuning(double, char, char) {}
   void MTS_FilterNoteMultiChannel(bool, char, char){}
   void MTS_ClearNoteFilterMultiChannel(char) {}

   // Client implementation
   void MTS_RegisterClient() {
      LOGFN;
      connectToMemory();
      (*numClients)++;
   }
   void MTS_DeregisterClient() {
      LOGFN;
      (*numClients)--;
   }
   // Don't implement note filtering
   bool MTS_ShouldFilterNote(char, char) { return false; }
   bool MTS_ShouldFilterNoteMultiChannel(char, char) { return false; }

   const double *MTS_GetTuningTable() { return &tuning[0]; }
   const double *MTS_GetMultiChannelTuningTable(char) { return &tuning[0]; }
   bool MTS_UseMultiChannelTuning(char) { return false; }
   const char * MTS_GetScaleName() { return scaleName; }

}
