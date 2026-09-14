/***************************************************************
 * Name:      grpar.cpp
 * Purpose:   EDID group arrays
 * Author:    Tomasz Pawlak (tomasz.pawlak@wp.eu)
 * Copyright: Tomasz Pawlak (C) 2021-2025
 * License:   GPLv3+
 **************************************************************/

#include "debug.h"
#include "rcdunits.h"
#ifndef idGRP_AR
   #error "grpar.cpp: missing unit ID"
#endif
#define RCD_UNIT idGRP_AR
#include "rcode/rcode.h"

#include "wxedid_rcd_scope.h"

RCD_AUTOGEN_DEFINE_UNIT

#include "EDID_class.h"


//WX_DEFINE_OBJARRAY: no-op with wxc_PtrArray (see wxcompat.h)

//group array: base class
void GroupAr_cl::Empty() {
   for (u32_t idx=0; idx<GetCount(); idx++) {
      delete Item(idx);
   }
   wxArGroup_cl::Empty();
}

void GroupAr_cl::Append(edi_grp_cl* pgrp) {
   pgrp->setArrayIdx(this, GetCount() );
   Add(pgrp);
}

void GroupAr_cl::UpdateRelOffs(u32_t idx, u32_t abs_offs, u32_t rel_offs) {
   edi_grp_cl *pgrp;
   u32_t       datsz;
   u32_t       n_grp;

   n_grp = GetCount();

   for (; idx<n_grp; ++idx) {
      pgrp      = Item(idx);

      pgrp->setAbsOffs(abs_offs);
      pgrp->setRelOffs(rel_offs);
      pgrp->setIndex  (idx);

      datsz     = pgrp->getDataSize();
      rel_offs += datsz;
      abs_offs += datsz;
   }
}

//if b_updt_soffs == false, don't update subgroups' relative offsets,
//if b_updt_soffs == true, update all subgroups' relative offsets, basing on abs_offs
void GroupAr_cl::UpdateAbsOffs(u32_t idx, u32_t abs_offs, bool b_updt_soffs) {
   edi_grp_cl *pgrp;
   u32_t       datsz;
   u32_t       rel_offs;
   u32_t       n_grp;

   n_grp     = GetCount();
   rel_offs  = abs_offs;
   rel_offs %= sizeof(edid_t);

   for (; idx<n_grp; ++idx) {
      bool  b_soffs;
      pgrp     = Item(idx);
      b_soffs  = b_updt_soffs;
      b_soffs &= (pgrp->getSubGrpCount() > 0);

      if (b_soffs) {
         edi_grp_cl *psubg;
         GroupAr_cl *sub_ar;
         u32_t       sub_rel;
         u32_t       sub_abs;

         sub_ar   = pgrp  ->getSubGrpAr();
         psubg    = sub_ar->Item(0);
         sub_rel  = psubg ->getRelOffs();
         sub_abs  = abs_offs;
         sub_abs += sub_rel;
         sub_ar->UpdateRelOffs(0, sub_abs, sub_rel);
      }

      pgrp->setAbsOffs(abs_offs);
      pgrp->setRelOffs(rel_offs);
      pgrp->setIndex(idx);

      datsz     = pgrp->getTotalSize();
      abs_offs += datsz;
      rel_offs += datsz;
   }
}

edi_grp_cl* GroupAr_cl::base_Cut(u32_t idx) {
   u32_t        abs_offs;
   edi_grp_cl  *pgrp;

   pgrp     = Detach(idx);
   abs_offs = pgrp->getAbsOffs();

   if (idx < GetCount()) {
      UpdateAbsOffs(idx, abs_offs);
   }
   CalcDataSZ(NULL);

   pgrp->setParentGrp(NULL);
   pgrp->setArrayIdx(NULL, -1);

   return pgrp;
}

void GroupAr_cl::base_Paste(u32_t idx, edi_grp_cl* pgrp) {
   u32_t       abs_offs;
   gtid_t      tid_src;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;
   GroupAr_cl *sub_ar;

   dst_grp  = Item(idx);
   abs_offs = dst_grp->getAbsOffs();

   pgrp->setArrayIdx(this, idx);

   sub_ar = pgrp->getSubGrpAr();
   if (sub_ar != NULL) {
      sub_ar->setParentArray(this);
   }

   //base EDID: inherit T_P_HOLDER flag (currently only for ID_DMT2)
   tid_src = pgrp->getTypeID();
   tid_dst = dst_grp->getTypeID();

   tid_src.t_p_holder = tid_dst.t_p_holder;
   pgrp->setTypeID(tid_src);

   Detach(idx);
   delete dst_grp;

   Insert(pgrp, idx);
   UpdateAbsOffs(idx, abs_offs);
   CalcDataSZ(NULL);
}

void GroupAr_cl::base_Delete(u32_t idx) {
   u32_t       abs_offs;
   edi_grp_cl *pgrp;

   pgrp     = Item(idx);
   abs_offs = pgrp->getAbsOffs();

   Detach(idx);
   delete pgrp;

   if (idx < GetCount()) {
      UpdateAbsOffs(idx, abs_offs);
   }
   CalcDataSZ(NULL);
}

void GroupAr_cl::base_MoveUp(u32_t idx) {
   //MoveUp(idx) == MoveDn(idx-1)
   idx -- ;
   base_MoveDn(idx);
}

void GroupAr_cl::base_MoveDn(u32_t idx) {
   u32_t        abs_offs;
   edi_grp_cl  *pgrp;

   pgrp      = Detach(idx);
   abs_offs  = pgrp->getAbsOffs();
   idx      ++ ;
   Insert(pgrp, idx);
   idx      -- ; //next group becomes prev. group

   UpdateAbsOffs(idx, abs_offs);
}

void GroupAr_cl::base_InsertUp(u32_t idx, edi_grp_cl* pgrp) {
   u32_t       abs_offs;
   edi_grp_cl *dst_grp;

   dst_grp = Item(idx);
   abs_offs = dst_grp->getAbsOffs();

   pgrp->setArrayIdx(this, idx);

   Insert(pgrp, idx);
   UpdateAbsOffs(idx, abs_offs);
   CalcDataSZ(NULL);
}

void GroupAr_cl::base_InsertDn(u32_t idx, edi_grp_cl* pgrp) {
   u32_t  n_grp;
   //InsertDn(idx) == InsertUp(idx+1)
   idx  ++ ;
   n_grp = GetCount();
   if (idx < n_grp) {
      base_InsertUp(idx, pgrp);
      return;
   }
   //insert after last group
   idx = (n_grp -1);
   base_InsertUp(idx, pgrp);
   base_MoveDn(idx);
}

bool GroupAr_cl::base_CanMoveUp(u32_t idx) {
   edi_grp_cl *pgrp;
   bool        p_holder;
   gtid_t      tid;
   bool        bret;
   u32_t       maxidx = GetCount();

   if ( (idx == 0) || (idx >= maxidx) ) return false;

   //group is not fixed
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();
   bret  = ! tid.t_gp_fixed;
   if (! bret) return false;

   p_holder = tid.t_p_holder;

   //previous group is not fixed
   idx  -- ;
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();

   bret  = ( p_holder &&  tid.t_p_holder); //can move place holders
   bret |= (!p_holder && !tid.t_p_holder); //can move alt. desc

   return bret;
}

bool GroupAr_cl::base_CanMoveDn(u32_t idx) {
   bool        p_holder;
   edi_grp_cl *pgrp;
   gtid_t      tid;
   bool        bret;
   u32_t       maxidx = GetCount();

   if (idx >= (maxidx -1) ) return false;

   //group is not fixed
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();
   bret  = ! tid.t_gp_fixed;
   if (! bret) return false;

   p_holder = tid.t_p_holder;

   //next group is not fixed
   idx  ++ ;
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();

   bret  = ( p_holder &&  tid.t_p_holder); //can move place holders
   bret |= (!p_holder && !tid.t_gp_fixed); //can move alt. desc

   return bret;
}

bool GroupAr_cl::base_CanInsert(edi_grp_cl* pgrp) {
   i32_t bsz;
   bool  bret;

   bsz  = pgrp->getTotalSize();
   bret = (bsz <= free_sz);

   //parent block free space
   if (parent_ar != NULL) {
      bret &= parent_ar->CanInsert(pgrp);
   }
   return bret;
}

bool GroupAr_cl::CanInsInto(u32_t idx, edi_grp_cl* pgrp) {
   bool        bret;
   gtid_t      tid_src;
   gtid_t      tid_dst;
   GroupAr_cl *dst_ar;
   edi_grp_cl *grp_dst;

   grp_dst = Item(idx);
   dst_ar  = grp_dst->getSubGrpAr();
   if (NULL == dst_ar) return false;

   //check subgroup free space, then parent array (*this*) free space
   bret = dst_ar->CanInsert(pgrp);
   if (! bret) return bret;

   tid_dst = grp_dst->getTypeID();
   tid_src = pgrp->getTypeID();

   //subg_id must match, src must be a sub-group
   bret  = (tid_src.subg_id == tid_dst.subg_id);
   bret &=  tid_src.t_sub_gp;

   return bret;
}

bool GroupAr_cl::CanPaste(u32_t idx, edi_grp_cl* src_grp) {
   bool        bret;
   edi_grp_cl *dst_grp;
   gtid_t      tid_src;
   gtid_t      tid_dst;
   i32_t       src_bsz;
   i32_t       dst_bsz;

   dst_grp = Item(idx);
   tid_dst = dst_grp->getTypeID();
   tid_src = src_grp->getTypeID();
   bret    = (tid_dst.t32 == tid_src.t32); //type must match
   if (bret) goto _chk_size;

   //alternatively, if both src and dest are sub-groups, subg_id must match:
   //this allows to exchange matching sub-grps between different parent grps
   bret  =  tid_src.t_sub_gp;
   bret &=  tid_dst.t_sub_gp;
   bret &= (tid_src.subg_id == tid_dst.subg_id);
   if (! bret) return bret;

_chk_size:
   //DBC blocks of the same type can have different sizes
   src_bsz  = src_grp->getTotalSize();
   dst_bsz  = dst_grp->getTotalSize();

   if (src_bsz > dst_bsz) {
      src_bsz -= dst_bsz;
      bret    &= (src_bsz <= free_sz);

      //parent block free space
      if (parent_ar != NULL) {
         dst_bsz  = parent_ar->getFreeSpace();
         bret    &= (src_bsz <= dst_bsz);
      }
   }
   return bret;
}

bool GroupAr_cl::CanCut(u32_t idx) {
   bool        bret;
   edi_grp_cl *pgrp;
   gtid_t      tid;

   pgrp = Item(idx);
   tid  = pgrp->getTypeID();
   bret = ! (tid.t_gp_fixed | tid.t_p_holder);

   return bret;
}

bool GroupAr_cl::CanDelete(u32_t idx) {
   bool        bret;
   edi_grp_cl *pgrp;
   gtid_t      tid;

   pgrp = Item(idx);
   tid  = pgrp->getTypeID();
   bret = ! (tid.t_gp_fixed | tid.t_p_holder);

   return bret;
}

void GroupAr_cl::base_CalcDataSZ(i32_t blk_sz, i32_t hdr_sz) {
   u32_t       n_grp;
   u32_t       idx_grp;
   u32_t       idx_subg;
   edi_grp_cl *pgrp;

   used_sz = hdr_sz;
   n_grp   = GetCount();

   for (idx_grp=0; idx_grp<n_grp; ++idx_grp) {
      u32_t       dat_sz;
      edi_grp_cl *p_subg;

      pgrp     = Item(idx_grp);
      dat_sz   = pgrp->getDataSize();
      used_sz += dat_sz;

      for (idx_subg=0; idx_subg<pgrp->getSubGrpCount(); ++idx_subg) {

         p_subg   = pgrp->getSubGroup(idx_subg);
         dat_sz   = p_subg->getDataSize();
         used_sz += dat_sz;
      }
   }

   free_sz  = blk_sz;
   free_sz -= used_sz;
}

//EDID
#pragma GCC diagnostic ignored "-Wunused-parameter"
void EDID_GrpAr_cl::CalcDataSZ(edi_grp_cl *pgrp) {
   //blk size -1 chsum byte, -1 num_of_extensions byte
   base_CalcDataSZ( sizeof(edid_t) -2);
}
#pragma GCC diagnostic warning "-Wunused-parameter"

bool EDID_GrpAr_cl::TypeSizeChk(edi_grp_cl* pgrp) {
   bool        bret;
   gtid_t      tid;

   bret = CanInsert(pgrp);
   if (! bret) return bret;

   //must be a DTD or alt. descriptor
   tid   = pgrp->getTypeID();
   bret  = ((tid.t32 & ID_PARENT_MASK) >= ID_UNK);
   bret &= ((tid.t32 & ID_PARENT_MASK) <= ID_DTD);

   return bret;
}

bool EDID_GrpAr_cl::CanInsertUp(u32_t idx, edi_grp_cl* pgrp) {
   bool        bret;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;

   bret = TypeSizeChk(pgrp);
   if (! bret) return bret;

   dst_grp = Item(idx);
   tid_dst = dst_grp->getTypeID();
   bret    = ! tid_dst.t_gp_fixed; //block @idx not fixed (ID_BED..ID_ETM)
   bret   &= ! tid_dst.t_p_holder; //not ID_DMT2

   return bret;
}

bool EDID_GrpAr_cl::CanInsertDn(u32_t idx, edi_grp_cl* pgrp) {
   bool  bret;

   bret  = TypeSizeChk(pgrp);
   bret &= (idx > 12); //last STI @idx=13 or a descriptor

   return bret;
}

//CTA and DisplayID extension groups array
bool CEA_GrpAr_cl::IsDisplayID() {
   if (GetCount() == 0) return false;
   return ((Item(0)->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID);
}

i32_t CEA_GrpAr_cl::DisplayIDInsertSpace() {
   i32_t available = free_sz;
   if ((GetCount() > 1) &&
       ((Item(GetCount() - 1)->getTypeID().t32 & ID_PARENT_MASK) ==
        ID_DISPLAYID_PADDING)) {
      available += Item(GetCount() - 1)->getTotalSize();
   }
   return available;
}

bool CEA_GrpAr_cl::DisplayIDTypeCheck(edi_grp_cl* pgrp) {
   return (pgrp != NULL) &&
      ((pgrp->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_DB) &&
      (static_cast<i32_t>(pgrp->getTotalSize()) <= DisplayIDInsertSpace());
}

void CEA_GrpAr_cl::DisplayIDReserve(u32_t size) {
   u32_t count = GetCount();
   if ((count > 1) &&
       ((Item(count - 1)->getTypeID().t32 & ID_PARENT_MASK) ==
        ID_DISPLAYID_PADDING)) {
      edi_grp_cl* padding = Item(count - 1);
      u32_t padding_size = padding->getTotalSize();
      if (padding_size > size) {
         padding->setDataSize(padding_size - size);
         CalcDataSZ(NULL);
         return;
      }
      delete base_Cut(count - 1);
      size -= padding_size;
      if (size == 0) return;
   }

   Item(0)->getInstPtr()[2] += size;
   CalcDataSZ(NULL);
}

void CEA_GrpAr_cl::DisplayIDRelease(u32_t size) {
   u32_t count = GetCount();
   if ((count > 1) &&
       ((Item(count - 1)->getTypeID().t32 & ID_PARENT_MASK) ==
        ID_DISPLAYID_PADDING)) {
      edi_grp_cl* padding = Item(count - 1);
      padding->setDataSize(padding->getTotalSize() + size);
   } else {
      Item(0)->getInstPtr()[2] -= size;
   }
   CalcDataSZ(NULL);
}

bool CEA_GrpAr_cl::CanMoveUp(u32_t idx) {
   if (IsDisplayID()) {
      if ((idx <= 1) || (idx >= GetCount())) return false;
      return ((Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_DB);
   }
   edi_grp_cl *pgrp;
   bool        b_DTD;
   gtid_t      tid;
   bool        bret;
   u32_t       maxidx = GetCount();

   if ( (idx == 0) || (idx >= maxidx) ) return false;

   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();
   bret  = ! tid.t_gp_fixed;
   if (! bret) return false;

   b_DTD = (ID_DTD == tid.base_id);

   idx  -- ;
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();
   bret  = ! tid.t_gp_fixed; //prev. grp not fixed (CEA header)
   if (! bret) return false;

   if (b_DTD) {
      //DTD and previous group is DTD
      bret = (ID_DTD == tid.base_id);
      return bret;
   }
   //DBC
   return true;
}

bool CEA_GrpAr_cl::CanMoveDn(u32_t idx) {
   if (IsDisplayID()) {
      if ((idx == 0) || (idx + 1 >= GetCount())) return false;
      return ((Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_DB) &&
         ((Item(idx + 1)->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_DB);
   }
   bool        bret;
   gtid_t      tid;
   edi_grp_cl *pgrp;

   bret = base_CanMoveDn(idx);
   if (! bret) return bret;

   pgrp = Item(idx);
   tid  = pgrp->getTypeID();

   if (ID_DTD == tid.base_id) return true;

   //DBC and next group is not a DTD
   idx  ++ ;
   pgrp  = Item(idx);
   tid   = pgrp->getTypeID();
   bret  = (ID_DTD != tid.base_id);

   return bret;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void CEA_GrpAr_cl::CalcDataSZ(edi_grp_cl *pgrp) {
   //blk size -1 chksum byte
   base_CalcDataSZ( sizeof(edid_t) -1);
}
#pragma GCC diagnostic warning "-Wunused-parameter"

bool CEA_GrpAr_cl::CanInsertUp(u32_t idx, edi_grp_cl* pgrp) {
   if (IsDisplayID()) {
      if ((idx == 0) || (idx >= GetCount())) return false;
      return DisplayIDTypeCheck(pgrp);
   }
   bool        bret;
   gtid_t      tid;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;

   bret = CanInsert(pgrp);
   if (! bret) return bret;

   dst_grp = Item(idx);
   tid_dst  = dst_grp->getTypeID();
   tid      = pgrp->getTypeID();

   //types allowed for insertion
   bret  = ! tid.t_sub_gp;       //not a sub-group
   bret &= ! tid_dst.t_gp_fixed; //block @idx not fixed (CHD)
   bret &= (ID_DTD <= (tid.t32 & ID_PARENT_MASK));
   if (! bret) return bret;

   if (ID_DTD != tid.base_id) {
      //not a DTD:
      //can insert only above existing DTD
      idx      -- ;
      dst_grp  = Item(idx);
      tid_dst  = dst_grp->getTypeID();
      bret     = (ID_DTD != tid.base_id);

      return bret;
   }
   //Insert DTD: only above existing DTD
   bret = (ID_DTD == tid_dst.base_id);

   return bret;
}

bool CEA_GrpAr_cl::CanInsertDn(u32_t idx, edi_grp_cl* pgrp) {
   if (IsDisplayID()) {
      if ((idx == 0) || (idx >= GetCount())) return false;
      if ((Item(idx)->getTypeID().t32 & ID_PARENT_MASK) == ID_DISPLAYID_PADDING)
         return false;
      return DisplayIDTypeCheck(pgrp);
   }
   bool        bret;
   gtid_t      tid;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;

   bret = CanInsert(pgrp);
   if (! bret) return bret;

   dst_grp = Item(idx);
   tid_dst  = dst_grp->getTypeID();
   tid      = pgrp->getTypeID();

   //types allowed for insertion
   bret  = ! tid.t_sub_gp;        //not a sub-group
   bret &= (ID_DTD <= (tid.t32 & ID_PARENT_MASK));
   if (! bret) return bret;

   if (ID_DTD == tid.base_id) {
      //DTD:
      //can insert after existing DTD
      if (ID_DTD == tid_dst.base_id) return true;

      bret  = true;
      idx  ++ ;

      if (idx < GetCount() ) {
         //can insert if next block is a DTD
         dst_grp = Item(idx);
         tid_dst = dst_grp->getTypeID();
         bret    = (ID_DTD == tid_dst.base_id);
      }
      return bret;
   }
   //not a DTD:
   //can insert only if block@idx is not a DTD
   bret  = (ID_DTD != tid_dst.base_id);

   return bret;
}

edi_grp_cl* CEA_GrpAr_cl::Cut(u32_t idx) {
   if (! IsDisplayID()) return base_Cut(idx);
   u32_t size = Item(idx)->getTotalSize();
   edi_grp_cl* result = base_Cut(idx);
   DisplayIDRelease(size);
   return result;
}

void CEA_GrpAr_cl::Delete(u32_t idx) {
   if (! IsDisplayID()) {
      base_Delete(idx);
      return;
   }
   edi_grp_cl* group = Cut(idx);
   delete group;
}

void CEA_GrpAr_cl::InsertUp(u32_t idx, edi_grp_cl* pgrp) {
   if (IsDisplayID()) DisplayIDReserve(pgrp->getTotalSize());
   base_InsertUp(idx, pgrp);
}

void CEA_GrpAr_cl::InsertDn(u32_t idx, edi_grp_cl* pgrp) {
   if (IsDisplayID()) DisplayIDReserve(pgrp->getTotalSize());
   base_InsertDn(idx, pgrp);
}

//DBC sub-groups array
void SubGrpAr_cl::CalcDataSZ(edi_grp_cl *pgrp) {
   i32_t   ahf_sz;
   gtid_t  tid;
   u32_t   hdr_sz;

   ahf_sz  = pgrp->getAHF_Size();
   hdr_sz  = 1;
   hdr_sz += ahf_sz;
   tid     = pgrp->getTypeID();

   if ((tid.t32 & ID_CEA_ET_MASK) != 0) hdr_sz ++ ; //+1 for ETag code

   base_CalcDataSZ(32, hdr_sz);
}

bool SubGrpAr_cl::doTypeMatch(u32_t idx, edi_grp_cl* pgrp) {
   bool        bret;
   bool        b_no_mv;
   gtid_t      tid_src;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;
   edi_grp_cl *par_grp;

   bret = CanInsert(pgrp);
   if (! bret) return bret;

   dst_grp = Item(idx);

   tid_src = pgrp->getTypeID();
   tid_dst = dst_grp->getTypeID();
   b_no_mv = tid_dst.t_no_move;

   //group type must match:
   tid_dst.t32 &= (ID_SUBGRP_MASK | ID_PARENT_MASK);
   bret         = (tid_dst.t32 == (tid_src.t32 & (ID_SUBGRP_MASK | ID_PARENT_MASK)) );
   bret        &= tid_src.t_sub_gp; //must be a sub-group

   //alternatively, parent->subg_id must match:
   //this allows to exchange matching sub-grps between different parent grps
   //currently only for base EDID::STI -> T8VTDB
   par_grp = dst_grp->getParentGrp();
   if (par_grp != NULL) {

      tid_dst = par_grp->getTypeID();
      bret   |= (tid_src.subg_id == tid_dst.subg_id);
      bret   &= (ID_INVALID      != tid_dst.subg_id); //subg_id must be defined
   }

   bret &= ! b_no_mv;

   return bret;
}

bool SubGrpAr_cl::CanInsertUp(u32_t idx, edi_grp_cl* pgrp) {
   bool  bret;

   bret = doTypeMatch(idx, pgrp);
   return bret;
}

bool SubGrpAr_cl::CanInsertDn(u32_t idx, edi_grp_cl* pgrp) {
   bool  bret;

   bret = doTypeMatch(idx, pgrp);
   return bret;
}

bool SubGrpAr_cl::CanMoveUp(u32_t idx) {
   bool   bret   = false;
   u32_t  maxidx = GetCount();

   if ( (idx > 0) && (idx < maxidx) ) {
      edi_grp_cl *pgrp;
      gtid_t      tid;

      //group is not fixed and moveable
      pgrp  = Item(idx);
      tid   = pgrp->getTypeID();
      bret  = ! tid.t_gp_fixed;
      bret &= ! tid.t_no_move;
      if (! bret) return false;

      //previous is not fixed and moveable
      idx  -- ;
      pgrp  = Item(idx);
      tid   = pgrp->getTypeID();

      bret  = ! tid.t_gp_fixed;
      bret &= ! tid.t_no_move;
   }
   return bret;
}

bool SubGrpAr_cl::CanMoveDn(u32_t idx) {
   bool   bret   = false;
   u32_t  maxidx = GetCount();

   if (idx < (maxidx -1) ) {
      edi_grp_cl *pgrp;
      gtid_t      tid;

      //group is not fixed and not place holder
      pgrp  = Item(idx);
      tid   = pgrp->getTypeID();
      bret  = ! tid.t_gp_fixed;
      bret &= ! tid.t_no_move;
      if (! bret) return false;

      //next group is not fixed and not place holder
      idx  ++ ;
      pgrp  = Item(idx);
      tid   = pgrp->getTypeID();

      bret  = ! tid.t_gp_fixed;
      bret &= ! tid.t_no_move;
   }
   return bret;
}

edi_grp_cl* SubGrpAr_cl::Cut(u32_t idx) {
   u32_t        rel_offs;
   u32_t        abs_offs;
   u32_t        parent_idx;
   edi_grp_cl  *parent;
   edi_grp_cl  *pgrp;

   pgrp     = Detach(idx);
   abs_offs = pgrp->getAbsOffs();
   rel_offs = pgrp->getRelOffs();
   parent   = pgrp->getParentGrp();

   if (idx < GetCount()) {
      UpdateRelOffs(idx, abs_offs, rel_offs);
   }
   CalcDataSZ(parent);
   parent->setDataSize(used_sz);

   abs_offs   = parent->getAbsOffs();
   parent_idx = parent->getParentArIdx();
   parent_ar  ->UpdateAbsOffs(parent_idx, abs_offs, false);
   parent_ar  ->CalcDataSZ(NULL);

   pgrp->setParentGrp(NULL);
   pgrp->setArrayIdx(NULL, -1);

   return pgrp;
}

void SubGrpAr_cl::doInsert(u32_t idx, edi_grp_cl* pgrp, edi_grp_cl* parent) {
   u32_t       abs_offs;
   u32_t       rel_offs;
   u32_t       parent_idx;
   gtid_t      tid_src;
   gtid_t      tid_dst;
   edi_grp_cl *dst_grp;

   if (NULL == parent) {
      dst_grp  = Item(idx);
      parent   = dst_grp->getParentGrp();
      abs_offs = dst_grp->getAbsOffs();
      rel_offs = dst_grp->getRelOffs();
   } else {
      abs_offs  = parent->getAbsOffs();
      rel_offs  = parent->getDataSize();
      abs_offs += rel_offs;
   }

   tid_src  = pgrp   ->getTypeID();
   tid_dst  = parent ->getTypeID();

   pgrp->setParentGrp(parent);
   pgrp->setArrayIdx(this, idx);

   Insert(pgrp, idx);
   CalcDataSZ(parent);
   UpdateRelOffs(idx, abs_offs, rel_offs);
   parent->setDataSize(used_sz);

   //DBC blocks of the same type can have different sizes
   abs_offs   = parent->getAbsOffs();
   parent_idx = parent->getParentArIdx();
   parent_ar  ->UpdateAbsOffs(parent_idx, abs_offs, false);
   parent_ar  ->CalcDataSZ(NULL);
}

void SubGrpAr_cl::Paste(u32_t idx, edi_grp_cl* pgrp) {

   doInsert(idx, pgrp, NULL);
   idx ++ ;
   Delete(idx);
}

void SubGrpAr_cl::Delete(u32_t idx) {
   u32_t       abs_offs;
   u32_t       rel_offs;
   u32_t       parent_idx;
   edi_grp_cl *pgrp;
   edi_grp_cl *parent;

   pgrp     = Item(idx);
   abs_offs = pgrp->getAbsOffs();
   rel_offs = pgrp->getRelOffs();
   parent   = pgrp->getParentGrp();

   Detach(idx);
   delete pgrp;

   if (idx < GetCount()) {
      UpdateRelOffs(idx, abs_offs, rel_offs);
   }
   CalcDataSZ(parent);
   parent->setDataSize(used_sz);

   abs_offs   = parent->getAbsOffs();
   parent_idx = parent->getParentArIdx();
   parent_ar  ->UpdateAbsOffs(parent_idx, abs_offs, false);
   parent_ar  ->CalcDataSZ(NULL);
}

void SubGrpAr_cl::MoveUp(u32_t idx) {
   //MoveUp(idx) == MoveDn(idx-1)
   idx -- ;
   MoveDn(idx);
}

void SubGrpAr_cl::MoveDn(u32_t idx) {
   u32_t        abs_offs;
   u32_t        rel_offs;
   edi_grp_cl  *pgrp;

   pgrp      = Detach(idx);
   abs_offs  = pgrp->getAbsOffs();
   rel_offs  = pgrp->getRelOffs();
   idx      ++ ;

   Insert  (pgrp, idx);
   pgrp->setIndex(idx);
   idx      -- ; //next group becomes prev. group

   UpdateRelOffs(idx, abs_offs, rel_offs);
}

void SubGrpAr_cl::InsertUp(u32_t idx, edi_grp_cl* pgrp) {
   doInsert(idx, pgrp, NULL);
}

void SubGrpAr_cl::InsertDn(u32_t idx, edi_grp_cl* pgrp) {
   u32_t  n_grp;
   //InsertDn(idx) == InsertUp(idx+1)
   idx  ++ ;
   n_grp = GetCount();
   if (idx < n_grp) {
      doInsert(idx, pgrp, NULL);
      return;
   }
   //insert after last sub-group
   idx = (n_grp -1);
   doInsert(idx, pgrp, NULL);
   MoveDn  (idx);
}

void SubGrpAr_cl::InsertInto(edi_grp_cl* parent, edi_grp_cl* pgrp) {
   doInsert(0, pgrp, parent);
}
