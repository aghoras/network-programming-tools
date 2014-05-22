#ifndef _LISTS_H
#define _LISTS_H

#include <list>
#include <assert.h>
#include <string.h>
using namespace std ;

//////////////////////////////////
// The following is a list which
// defines a safe handle list
//////////////////////////////////
template <class T>
class Handle_List{
   public:
      ~Handle_List();
      Handle_List(unsigned int uSize);
      T& operator[](unsigned int uIndex);        //index operator
      unsigned int GetSize(){return m_uSize;} //returns the number elements in this list
      unsigned int GetNewHandle();            //creates a new handle and returns it id
      void         FreeHandle(unsigned int uIndex); //marks a slot as invalid and returns it to the free list
      bool         IsGoodHandle(const unsigned int uIndex);
      unsigned int GetCount(){return m_uCount;}; //returns the number of allocated handles
   protected:
      unsigned m_uSize;
      unsigned m_uCount;
      struct DATA_CONTAINER{
         T     Element;
         bool  bValid;
      }*m_Data;
      list<int> m_FreeHandle_List;
};

////////////////
// Constructor
///////////////
template <class T>
Handle_List<T>::Handle_List(unsigned int uSize){
   //allocate new memory for the array
   m_Data=new DATA_CONTAINER [uSize];
   memset(m_Data,0,sizeof(DATA_CONTAINER)*uSize);
   //put all the handles on the free list
   m_uSize=uSize;
   m_uCount=0;
   while(uSize-->0){
      m_FreeHandle_List.push_front(uSize);
   }
}
///////////////////
// Deconstructor
///////////////////
template<class T>
Handle_List<T>::~Handle_List(){
   //free data memory
   delete [] m_Data;
   m_FreeHandle_List.clear();
   m_uCount=0;
}
/////////////////////////////////
// Returns the index of the next
// available handle
////////////////////////////////
template<class T>
unsigned int Handle_List<T>::GetNewHandle(){
   unsigned int uResults=0xFFFFFFFF;;
   //if there are no more free handles
   if(!m_FreeHandle_List.empty()){
      uResults=m_FreeHandle_List.front();  
      m_FreeHandle_List.pop_front();  
      m_Data[uResults].bValid=true;
      m_uCount++;
   }

   return uResults;
}
//////////////////////////////////
// Returns the handle to the
// free list
//////////////////////////////////
template<class T>
void Handle_List<T>::FreeHandle(const unsigned int uIndex){
   
   //check for bad handle
   if(!IsGoodHandle(uIndex)){
      assert(!"Bad Handle");
      return;
   }
   m_Data[uIndex].bValid=false;
   m_FreeHandle_List.push_front(uIndex);
   m_uCount--;
}
//////////////////////////////////
// index operator
//////////////////////////////////
template<class T>
T& Handle_List<T>::operator[](const unsigned int uIndex){
   
   //check for bad handle
   if(!IsGoodHandle(uIndex)){
      assert(!"Bad Handle");
      return (m_Data[0].Element);
   }
  return(m_Data[uIndex].Element);
}
//////////////////////////////////
// index operator
//////////////////////////////////
template<class T>
bool Handle_List<T>::IsGoodHandle(const unsigned int uIndex){
   
   //check for bad handle
   if(uIndex>m_uSize){
      return (false);
   }
   if(m_Data[uIndex].bValid==false){
      return (false);
   }
  return(true);
}
#endif
