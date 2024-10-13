case RISCVVector::BI__builtin_rvv_vget_v:
  PolicyAttrs = 3;
IsMasked = false;

      {
        ID = Intrinsic::vector_extract;
        auto *VecTy = cast<ScalableVectorType>(ResultType);
        auto *OpVecTy = cast<ScalableVectorType>(Ops[0]->getType());
        // Mask to only valid indices.
        unsigned MaxIndex = OpVecTy->getMinNumElements() / VecTy->getMinNumElements();
        assert(isPowerOf2_32(MaxIndex));
        Ops[1] = Builder.CreateZExt(Ops[1], Builder.getInt64Ty());
        Ops[1] = Builder.CreateAnd(Ops[1], MaxIndex - 1);
        Ops[1] = Builder.CreateMul(Ops[1],
                                   ConstantInt::get(Ops[1]->getType(),
                                                    VecTy->getMinNumElements()));
        IntrinsicTypes = {ResultType, Ops[0]->getType()};
        return Builder.CreateCall(CGM.getIntrinsic(ID, IntrinsicTypes), Ops, "");
      }
      break;
case RISCVVector::BI__builtin_rvv_vset_v:
  PolicyAttrs = 3;
IsMasked = false;

      {
        ID = Intrinsic::vector_insert;
        IntrinsicTypes = {ResultType, Ops[2]->getType()};
        auto *ResVecTy = cast<ScalableVectorType>(ResultType);
        auto *VecTy = cast<ScalableVectorType>(Ops[2]->getType());
        // Mask to only valid indices.
        unsigned MaxIndex = ResVecTy->getMinNumElements() / VecTy->getMinNumElements();
        assert(isPowerOf2_32(MaxIndex));
        Ops[1] = Builder.CreateZExt(Ops[1], Builder.getInt64Ty());
        Ops[1] = Builder.CreateAnd(Ops[1], MaxIndex - 1);
        Ops[1] = Builder.CreateMul(Ops[1],
                                   ConstantInt::get(Ops[1]->getType(),
                                                    VecTy->getMinNumElements()));
        std::swap(Ops[1], Ops[2]);
        return Builder.CreateCall(CGM.getIntrinsic(ID, IntrinsicTypes), Ops, "");
      }
      break;
case RISCVVector::BI__builtin_rvv_vlenb:
  PolicyAttrs = 3;
IsMasked = false;

    {
      LLVMContext &Context = CGM.getLLVMContext();
      llvm::MDBuilder MDHelper(Context);

      llvm::Metadata *Ops[] = {llvm::MDString::get(Context, "vlenb")};
      llvm::MDNode *RegName = llvm::MDNode::get(Context, Ops);
      llvm::Value *Metadata = llvm::MetadataAsValue::get(Context, RegName);
      llvm::Function *F =
        CGM.getIntrinsic(llvm::Intrinsic::read_register, {SizeTy});
      return Builder.CreateCall(F, Metadata);
    }
    break;
case RISCVVector::BI__builtin_rvv_vlmul_ext_v:
  PolicyAttrs = 3;
IsMasked = false;

        ID = Intrinsic::vector_insert;
        IntrinsicTypes = {ResultType, Ops[0]->getType()};
        Ops.push_back(llvm::PoisonValue::get(ResultType));
        std::swap(Ops[0], Ops[1]);
        Ops.push_back(ConstantInt::get(Int64Ty, 0));
        return Builder.CreateCall(CGM.getIntrinsic(ID, IntrinsicTypes), Ops, "");
      break;
case RISCVVector::BI__builtin_rvv_vlmul_trunc_v:
  PolicyAttrs = 3;
IsMasked = false;
 {
        ID = Intrinsic::vector_extract;
        IntrinsicTypes = {ResultType, Ops[0]->getType()};
        Ops.push_back(ConstantInt::get(Int64Ty, 0));
        return Builder.CreateCall(CGM.getIntrinsic(ID, IntrinsicTypes), Ops, "");
      } break;
case RISCVVector::BI__builtin_rvv_vreinterpret_v:
  PolicyAttrs = 3;
IsMasked = false;

        return Builder.CreateBitCast(Ops[0], ResultType);
      break;
case RISCVVector::BI__builtin_rvv_vundefined:
  PolicyAttrs = 3;
IsMasked = false;

        return llvm::PoisonValue::get(ResultType);
      break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vaadd_vx_tu:
  ID = Intrinsic::riscv_vaadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vaadd_vx_ta:
  ID = Intrinsic::riscv_vaadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vaadd_vx_tuma:
  ID = Intrinsic::riscv_vaadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vaadd_vx_tama:
  ID = Intrinsic::riscv_vaadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vaadd_vx_tumu:
  ID = Intrinsic::riscv_vaadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vaadd_vx_tamu:
  ID = Intrinsic::riscv_vaadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_tu:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_tu:
  ID = Intrinsic::riscv_vaaddu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_ta:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_ta:
  ID = Intrinsic::riscv_vaaddu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_tuma:
  ID = Intrinsic::riscv_vaaddu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_tama:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_tama:
  ID = Intrinsic::riscv_vaaddu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_tumu:
  ID = Intrinsic::riscv_vaaddu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vaaddu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vaaddu_vx_tamu:
  ID = Intrinsic::riscv_vaaddu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadc_vvm_tu:
case RISCVVector::BI__builtin_rvv_vadc_vxm_tu:
  ID = Intrinsic::riscv_vadc;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadc_vvm_ta:
case RISCVVector::BI__builtin_rvv_vadc_vxm_ta:
  ID = Intrinsic::riscv_vadc;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vadd_vx_tu:
  ID = Intrinsic::riscv_vadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vadd_vx_ta:
  ID = Intrinsic::riscv_vadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vadd_vx_tuma:
  ID = Intrinsic::riscv_vadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vadd_vx_tama:
  ID = Intrinsic::riscv_vadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vadd_vx_tumu:
  ID = Intrinsic::riscv_vadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vadd_vx_tamu:
  ID = Intrinsic::riscv_vadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_tu:
case RISCVVector::BI__builtin_rvv_vand_vx_tu:
  ID = Intrinsic::riscv_vand;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_ta:
case RISCVVector::BI__builtin_rvv_vand_vx_ta:
  ID = Intrinsic::riscv_vand;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_tuma:
case RISCVVector::BI__builtin_rvv_vand_vx_tuma:
  ID = Intrinsic::riscv_vand_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_tama:
case RISCVVector::BI__builtin_rvv_vand_vx_tama:
  ID = Intrinsic::riscv_vand_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_tumu:
case RISCVVector::BI__builtin_rvv_vand_vx_tumu:
  ID = Intrinsic::riscv_vand_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vand_vv_tamu:
case RISCVVector::BI__builtin_rvv_vand_vx_tamu:
  ID = Intrinsic::riscv_vand_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_tu:
case RISCVVector::BI__builtin_rvv_vasub_vx_tu:
  ID = Intrinsic::riscv_vasub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_ta:
case RISCVVector::BI__builtin_rvv_vasub_vx_ta:
  ID = Intrinsic::riscv_vasub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vasub_vx_tuma:
  ID = Intrinsic::riscv_vasub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_tama:
case RISCVVector::BI__builtin_rvv_vasub_vx_tama:
  ID = Intrinsic::riscv_vasub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vasub_vx_tumu:
  ID = Intrinsic::riscv_vasub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vasub_vx_tamu:
  ID = Intrinsic::riscv_vasub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_tu:
case RISCVVector::BI__builtin_rvv_vasubu_vx_tu:
  ID = Intrinsic::riscv_vasubu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_ta:
case RISCVVector::BI__builtin_rvv_vasubu_vx_ta:
  ID = Intrinsic::riscv_vasubu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vasubu_vx_tuma:
  ID = Intrinsic::riscv_vasubu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_tama:
case RISCVVector::BI__builtin_rvv_vasubu_vx_tama:
  ID = Intrinsic::riscv_vasubu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vasubu_vx_tumu:
  ID = Intrinsic::riscv_vasubu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vasubu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vasubu_vx_tamu:
  ID = Intrinsic::riscv_vasubu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vcompress_vm_tu:
  ID = Intrinsic::riscv_vcompress;
  PolicyAttrs = 2;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vcompress_vm_ta:
  ID = Intrinsic::riscv_vcompress;
  PolicyAttrs = 3;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vcpop_m:
  ID = Intrinsic::riscv_vcpop;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vcpop_m_m:
  ID = Intrinsic::riscv_vcpop_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[0]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_tu:
case RISCVVector::BI__builtin_rvv_vdiv_vx_tu:
  ID = Intrinsic::riscv_vdiv;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_ta:
case RISCVVector::BI__builtin_rvv_vdiv_vx_ta:
  ID = Intrinsic::riscv_vdiv;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_tuma:
case RISCVVector::BI__builtin_rvv_vdiv_vx_tuma:
  ID = Intrinsic::riscv_vdiv_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_tama:
case RISCVVector::BI__builtin_rvv_vdiv_vx_tama:
  ID = Intrinsic::riscv_vdiv_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_tumu:
case RISCVVector::BI__builtin_rvv_vdiv_vx_tumu:
  ID = Intrinsic::riscv_vdiv_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdiv_vv_tamu:
case RISCVVector::BI__builtin_rvv_vdiv_vx_tamu:
  ID = Intrinsic::riscv_vdiv_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_tu:
case RISCVVector::BI__builtin_rvv_vdivu_vx_tu:
  ID = Intrinsic::riscv_vdivu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_ta:
case RISCVVector::BI__builtin_rvv_vdivu_vx_ta:
  ID = Intrinsic::riscv_vdivu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vdivu_vx_tuma:
  ID = Intrinsic::riscv_vdivu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_tama:
case RISCVVector::BI__builtin_rvv_vdivu_vx_tama:
  ID = Intrinsic::riscv_vdivu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vdivu_vx_tumu:
  ID = Intrinsic::riscv_vdivu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vdivu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vdivu_vx_tamu:
  ID = Intrinsic::riscv_vdivu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vfadd_vf_tu:
  ID = Intrinsic::riscv_vfadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vfadd_vf_ta:
  ID = Intrinsic::riscv_vfadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfadd_vf_tuma:
  ID = Intrinsic::riscv_vfadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vfadd_vf_tama:
  ID = Intrinsic::riscv_vfadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfadd_vf_tumu:
  ID = Intrinsic::riscv_vfadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfadd_vf_tamu:
  ID = Intrinsic::riscv_vfadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_tu:
  ID = Intrinsic::riscv_vfclass;
  PolicyAttrs = 2;
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_ta:
  ID = Intrinsic::riscv_vfclass;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_tuma:
  ID = Intrinsic::riscv_vfclass_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_tama:
  ID = Intrinsic::riscv_vfclass_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_tumu:
  ID = Intrinsic::riscv_vfclass_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfclass_v_tamu:
  ID = Intrinsic::riscv_vfclass_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tu:
  ID = Intrinsic::riscv_vfcvt_f_x_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_ta:
  ID = Intrinsic::riscv_vfcvt_f_x_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tuma:
  ID = Intrinsic::riscv_vfcvt_f_x_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tama:
  ID = Intrinsic::riscv_vfcvt_f_x_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tumu:
  ID = Intrinsic::riscv_vfcvt_f_x_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_x_v_tamu:
  ID = Intrinsic::riscv_vfcvt_f_x_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tu:
  ID = Intrinsic::riscv_vfcvt_f_xu_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_ta:
  ID = Intrinsic::riscv_vfcvt_f_xu_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tuma:
  ID = Intrinsic::riscv_vfcvt_f_xu_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tama:
  ID = Intrinsic::riscv_vfcvt_f_xu_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tumu:
  ID = Intrinsic::riscv_vfcvt_f_xu_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_f_xu_v_tamu:
  ID = Intrinsic::riscv_vfcvt_f_xu_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tu:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_ta:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tuma:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tama:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tumu:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_x_f_v_tamu:
  ID = Intrinsic::riscv_vfcvt_rtz_x_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tu:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_ta:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tuma:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tama:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tumu:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_rtz_xu_f_v_tamu:
  ID = Intrinsic::riscv_vfcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tu:
  ID = Intrinsic::riscv_vfcvt_x_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_ta:
  ID = Intrinsic::riscv_vfcvt_x_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tuma:
  ID = Intrinsic::riscv_vfcvt_x_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tama:
  ID = Intrinsic::riscv_vfcvt_x_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tumu:
  ID = Intrinsic::riscv_vfcvt_x_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_x_f_v_tamu:
  ID = Intrinsic::riscv_vfcvt_x_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tu:
  ID = Intrinsic::riscv_vfcvt_xu_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_ta:
  ID = Intrinsic::riscv_vfcvt_xu_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tuma:
  ID = Intrinsic::riscv_vfcvt_xu_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tama:
  ID = Intrinsic::riscv_vfcvt_xu_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tumu:
  ID = Intrinsic::riscv_vfcvt_xu_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfcvt_xu_f_v_tamu:
  ID = Intrinsic::riscv_vfcvt_xu_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_tu:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_tu:
  ID = Intrinsic::riscv_vfdiv;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_ta:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_ta:
  ID = Intrinsic::riscv_vfdiv;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_tuma:
  ID = Intrinsic::riscv_vfdiv_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_tama:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_tama:
  ID = Intrinsic::riscv_vfdiv_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_tumu:
  ID = Intrinsic::riscv_vfdiv_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfdiv_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfdiv_vf_tamu:
  ID = Intrinsic::riscv_vfdiv_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfirst_m:
  ID = Intrinsic::riscv_vfirst;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfirst_m_m:
  ID = Intrinsic::riscv_vfirst_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[0]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_tu:
  ID = Intrinsic::riscv_vfmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_ta:
  ID = Intrinsic::riscv_vfmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_tuma:
  ID = Intrinsic::riscv_vfmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_tama:
  ID = Intrinsic::riscv_vfmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_tumu:
  ID = Intrinsic::riscv_vfmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmacc_vf_tamu:
  ID = Intrinsic::riscv_vfmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_tu:
  ID = Intrinsic::riscv_vfmadd;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_ta:
  ID = Intrinsic::riscv_vfmadd;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_tuma:
  ID = Intrinsic::riscv_vfmadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_tama:
  ID = Intrinsic::riscv_vfmadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_tumu:
  ID = Intrinsic::riscv_vfmadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmadd_vf_tamu:
  ID = Intrinsic::riscv_vfmadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmax_vf_tu:
  ID = Intrinsic::riscv_vfmax;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmax_vf_ta:
  ID = Intrinsic::riscv_vfmax;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmax_vf_tuma:
  ID = Intrinsic::riscv_vfmax_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmax_vf_tama:
  ID = Intrinsic::riscv_vfmax_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmax_vf_tumu:
  ID = Intrinsic::riscv_vfmax_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmax_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmax_vf_tamu:
  ID = Intrinsic::riscv_vfmax_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmerge_vfm_tu:
  ID = Intrinsic::riscv_vfmerge;
  PolicyAttrs = 2;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vfmerge_vfm_ta:
  ID = Intrinsic::riscv_vfmerge;
  PolicyAttrs = 3;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmin_vf_tu:
  ID = Intrinsic::riscv_vfmin;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmin_vf_ta:
  ID = Intrinsic::riscv_vfmin;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmin_vf_tuma:
  ID = Intrinsic::riscv_vfmin_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmin_vf_tama:
  ID = Intrinsic::riscv_vfmin_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmin_vf_tumu:
  ID = Intrinsic::riscv_vfmin_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmin_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmin_vf_tamu:
  ID = Intrinsic::riscv_vfmin_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_tu:
  ID = Intrinsic::riscv_vfmsac;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_ta:
  ID = Intrinsic::riscv_vfmsac;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_tuma:
  ID = Intrinsic::riscv_vfmsac_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_tama:
  ID = Intrinsic::riscv_vfmsac_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_tumu:
  ID = Intrinsic::riscv_vfmsac_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsac_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmsac_vf_tamu:
  ID = Intrinsic::riscv_vfmsac_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_tu:
  ID = Intrinsic::riscv_vfmsub;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_ta:
  ID = Intrinsic::riscv_vfmsub;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_tuma:
  ID = Intrinsic::riscv_vfmsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_tama:
  ID = Intrinsic::riscv_vfmsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_tumu:
  ID = Intrinsic::riscv_vfmsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmsub_vf_tamu:
  ID = Intrinsic::riscv_vfmsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_tu:
case RISCVVector::BI__builtin_rvv_vfmul_vf_tu:
  ID = Intrinsic::riscv_vfmul;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_ta:
case RISCVVector::BI__builtin_rvv_vfmul_vf_ta:
  ID = Intrinsic::riscv_vfmul;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfmul_vf_tuma:
  ID = Intrinsic::riscv_vfmul_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_tama:
case RISCVVector::BI__builtin_rvv_vfmul_vf_tama:
  ID = Intrinsic::riscv_vfmul_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfmul_vf_tumu:
  ID = Intrinsic::riscv_vfmul_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmul_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfmul_vf_tamu:
  ID = Intrinsic::riscv_vfmul_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmv_f_s:
  ID = Intrinsic::riscv_vfmv_f_s;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmv_s_f_tu:
case RISCVVector::BI__builtin_rvv_vfmv_s_x_tu:
  ID = Intrinsic::riscv_vfmv_s_f;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmv_s_f_ta:
case RISCVVector::BI__builtin_rvv_vfmv_s_x_ta:
  ID = Intrinsic::riscv_vfmv_s_f;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmv_v_f_tu:
  ID = Intrinsic::riscv_vfmv_v_f;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfmv_v_f_ta:
  ID = Intrinsic::riscv_vfmv_v_f;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_f_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_f_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_f_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_f_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_f_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_f_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_tu:
  ID = Intrinsic::riscv_vfncvt_f_x_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_ta:
  ID = Intrinsic::riscv_vfncvt_f_x_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_tuma:
  ID = Intrinsic::riscv_vfncvt_f_x_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_tama:
  ID = Intrinsic::riscv_vfncvt_f_x_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_tumu:
  ID = Intrinsic::riscv_vfncvt_f_x_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_x_w_tamu:
  ID = Intrinsic::riscv_vfncvt_f_x_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_tu:
  ID = Intrinsic::riscv_vfncvt_f_xu_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_ta:
  ID = Intrinsic::riscv_vfncvt_f_xu_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_tuma:
  ID = Intrinsic::riscv_vfncvt_f_xu_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_tama:
  ID = Intrinsic::riscv_vfncvt_f_xu_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_tumu:
  ID = Intrinsic::riscv_vfncvt_f_xu_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_f_xu_w_tamu:
  ID = Intrinsic::riscv_vfncvt_f_xu_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rod_f_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_rod_f_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_x_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_rtz_x_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_rtz_xu_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_rtz_xu_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_x_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_x_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_x_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_x_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_x_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_x_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_x_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_tu:
  ID = Intrinsic::riscv_vfncvt_xu_f_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_ta:
  ID = Intrinsic::riscv_vfncvt_xu_f_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_tuma:
  ID = Intrinsic::riscv_vfncvt_xu_f_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_tama:
  ID = Intrinsic::riscv_vfncvt_xu_f_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_tumu:
  ID = Intrinsic::riscv_vfncvt_xu_f_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfncvt_xu_f_w_tamu:
  ID = Intrinsic::riscv_vfncvt_xu_f_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_tu:
  ID = Intrinsic::riscv_vfnmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_ta:
  ID = Intrinsic::riscv_vfnmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_tuma:
  ID = Intrinsic::riscv_vfnmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_tama:
  ID = Intrinsic::riscv_vfnmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_tumu:
  ID = Intrinsic::riscv_vfnmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfnmacc_vf_tamu:
  ID = Intrinsic::riscv_vfnmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_tu:
  ID = Intrinsic::riscv_vfnmadd;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_ta:
  ID = Intrinsic::riscv_vfnmadd;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_tuma:
  ID = Intrinsic::riscv_vfnmadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_tama:
  ID = Intrinsic::riscv_vfnmadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_tumu:
  ID = Intrinsic::riscv_vfnmadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfnmadd_vf_tamu:
  ID = Intrinsic::riscv_vfnmadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_tu:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_tu:
  ID = Intrinsic::riscv_vfnmsac;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_ta:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_ta:
  ID = Intrinsic::riscv_vfnmsac;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_tuma:
  ID = Intrinsic::riscv_vfnmsac_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_tama:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_tama:
  ID = Intrinsic::riscv_vfnmsac_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_tumu:
  ID = Intrinsic::riscv_vfnmsac_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsac_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfnmsac_vf_tamu:
  ID = Intrinsic::riscv_vfnmsac_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_tu:
  ID = Intrinsic::riscv_vfnmsub;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_ta:
  ID = Intrinsic::riscv_vfnmsub;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_tuma:
  ID = Intrinsic::riscv_vfnmsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_tama:
  ID = Intrinsic::riscv_vfnmsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_tumu:
  ID = Intrinsic::riscv_vfnmsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfnmsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfnmsub_vf_tamu:
  ID = Intrinsic::riscv_vfnmsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_tu:
  ID = Intrinsic::riscv_vfrdiv;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_ta:
  ID = Intrinsic::riscv_vfrdiv;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_tuma:
  ID = Intrinsic::riscv_vfrdiv_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_tama:
  ID = Intrinsic::riscv_vfrdiv_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_tumu:
  ID = Intrinsic::riscv_vfrdiv_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrdiv_vf_tamu:
  ID = Intrinsic::riscv_vfrdiv_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_tu:
  ID = Intrinsic::riscv_vfrec7;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_ta:
  ID = Intrinsic::riscv_vfrec7;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_tuma:
  ID = Intrinsic::riscv_vfrec7_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_tama:
  ID = Intrinsic::riscv_vfrec7_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_tumu:
  ID = Intrinsic::riscv_vfrec7_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrec7_v_tamu:
  ID = Intrinsic::riscv_vfrec7_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmax_vs_tu:
  ID = Intrinsic::riscv_vfredmax;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmax_vs_ta:
  ID = Intrinsic::riscv_vfredmax;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmax_vs_tum:
  ID = Intrinsic::riscv_vfredmax_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmax_vs_tama:
case RISCVVector::BI__builtin_rvv_vfredmax_vs_tam:
  ID = Intrinsic::riscv_vfredmax_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmin_vs_tu:
  ID = Intrinsic::riscv_vfredmin;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmin_vs_ta:
  ID = Intrinsic::riscv_vfredmin;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmin_vs_tum:
  ID = Intrinsic::riscv_vfredmin_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredmin_vs_tama:
case RISCVVector::BI__builtin_rvv_vfredmin_vs_tam:
  ID = Intrinsic::riscv_vfredmin_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredosum_vs_tu:
  ID = Intrinsic::riscv_vfredosum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredosum_vs_ta:
  ID = Intrinsic::riscv_vfredosum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredosum_vs_tum:
  ID = Intrinsic::riscv_vfredosum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredosum_vs_tama:
case RISCVVector::BI__builtin_rvv_vfredosum_vs_tam:
  ID = Intrinsic::riscv_vfredosum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredusum_vs_tu:
  ID = Intrinsic::riscv_vfredusum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredusum_vs_ta:
  ID = Intrinsic::riscv_vfredusum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredusum_vs_tum:
  ID = Intrinsic::riscv_vfredusum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfredusum_vs_tama:
case RISCVVector::BI__builtin_rvv_vfredusum_vs_tam:
  ID = Intrinsic::riscv_vfredusum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tu:
  ID = Intrinsic::riscv_vfrsqrt7;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_ta:
  ID = Intrinsic::riscv_vfrsqrt7;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tuma:
  ID = Intrinsic::riscv_vfrsqrt7_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tama:
  ID = Intrinsic::riscv_vfrsqrt7_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tumu:
  ID = Intrinsic::riscv_vfrsqrt7_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsqrt7_v_tamu:
  ID = Intrinsic::riscv_vfrsqrt7_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_tu:
  ID = Intrinsic::riscv_vfrsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_ta:
  ID = Intrinsic::riscv_vfrsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_tuma:
  ID = Intrinsic::riscv_vfrsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_tama:
  ID = Intrinsic::riscv_vfrsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_tumu:
  ID = Intrinsic::riscv_vfrsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfrsub_vf_tamu:
  ID = Intrinsic::riscv_vfrsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tu:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tu:
  ID = Intrinsic::riscv_vfsgnj;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_ta:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_ta:
  ID = Intrinsic::riscv_vfsgnj;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tuma:
  ID = Intrinsic::riscv_vfsgnj_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tama:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tama:
  ID = Intrinsic::riscv_vfsgnj_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tumu:
  ID = Intrinsic::riscv_vfsgnj_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnj_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfsgnj_vf_tamu:
  ID = Intrinsic::riscv_vfsgnj_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tu:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tu:
  ID = Intrinsic::riscv_vfsgnjn;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_tu:
  ID = Intrinsic::riscv_vfsgnjn;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_ta:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_ta:
  ID = Intrinsic::riscv_vfsgnjn;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_ta:
  ID = Intrinsic::riscv_vfsgnjn;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tuma:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_tuma:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tama:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tama:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_tama:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tumu:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_tumu:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjn_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfsgnjn_vf_tamu:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfneg_v_tamu:
  ID = Intrinsic::riscv_vfsgnjn_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tu:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tu:
  ID = Intrinsic::riscv_vfsgnjx;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_tu:
  ID = Intrinsic::riscv_vfsgnjx;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_ta:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_ta:
  ID = Intrinsic::riscv_vfsgnjx;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_ta:
  ID = Intrinsic::riscv_vfsgnjx;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tuma:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_tuma:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tama:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tama:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_tama:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tumu:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_tumu:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfsgnjx_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfsgnjx_vf_tamu:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfabs_v_tamu:
  ID = Intrinsic::riscv_vfsgnjx_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.insert(Ops.begin() + 2, Ops[1]);
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[2]->getType(),
                            Ops.back()->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          // op1, po2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(), Ops[2]->getType()};
          Ops.insert(Ops.begin() + 2, Ops[1]);
          break;
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tu:
  ID = Intrinsic::riscv_vfslide1down;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_ta:
  ID = Intrinsic::riscv_vfslide1down;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tuma:
  ID = Intrinsic::riscv_vfslide1down_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tama:
  ID = Intrinsic::riscv_vfslide1down_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tumu:
  ID = Intrinsic::riscv_vfslide1down_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1down_vf_tamu:
  ID = Intrinsic::riscv_vfslide1down_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tu:
  ID = Intrinsic::riscv_vfslide1up;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_ta:
  ID = Intrinsic::riscv_vfslide1up;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tuma:
  ID = Intrinsic::riscv_vfslide1up_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tama:
  ID = Intrinsic::riscv_vfslide1up_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tumu:
  ID = Intrinsic::riscv_vfslide1up_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfslide1up_vf_tamu:
  ID = Intrinsic::riscv_vfslide1up_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_tu:
  ID = Intrinsic::riscv_vfsqrt;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_ta:
  ID = Intrinsic::riscv_vfsqrt;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_tuma:
  ID = Intrinsic::riscv_vfsqrt_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_tama:
  ID = Intrinsic::riscv_vfsqrt_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_tumu:
  ID = Intrinsic::riscv_vfsqrt_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsqrt_v_tamu:
  ID = Intrinsic::riscv_vfsqrt_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vfsub_vf_tu:
  ID = Intrinsic::riscv_vfsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vfsub_vf_ta:
  ID = Intrinsic::riscv_vfsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfsub_vf_tuma:
  ID = Intrinsic::riscv_vfsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vfsub_vf_tama:
  ID = Intrinsic::riscv_vfsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfsub_vf_tumu:
  ID = Intrinsic::riscv_vfsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfsub_vf_tamu:
  ID = Intrinsic::riscv_vfsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_tu:
  ID = Intrinsic::riscv_vfwadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_ta:
  ID = Intrinsic::riscv_vfwadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_tuma:
  ID = Intrinsic::riscv_vfwadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_tama:
  ID = Intrinsic::riscv_vfwadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_tumu:
  ID = Intrinsic::riscv_vfwadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwadd_vf_tamu:
  ID = Intrinsic::riscv_vfwadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_tu:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_tu:
  ID = Intrinsic::riscv_vfwadd_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_ta:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_ta:
  ID = Intrinsic::riscv_vfwadd_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_tuma:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_tuma:
  ID = Intrinsic::riscv_vfwadd_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_tama:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_tama:
  ID = Intrinsic::riscv_vfwadd_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_tumu:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_tumu:
  ID = Intrinsic::riscv_vfwadd_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwadd_wv_tamu:
case RISCVVector::BI__builtin_rvv_vfwadd_wf_tamu:
  ID = Intrinsic::riscv_vfwadd_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tu:
  ID = Intrinsic::riscv_vfwcvt_f_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_ta:
  ID = Intrinsic::riscv_vfwcvt_f_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_f_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tama:
  ID = Intrinsic::riscv_vfwcvt_f_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_f_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_f_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_f_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tu:
  ID = Intrinsic::riscv_vfwcvt_f_x_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_ta:
  ID = Intrinsic::riscv_vfwcvt_f_x_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_f_x_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tama:
  ID = Intrinsic::riscv_vfwcvt_f_x_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_f_x_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_x_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_f_x_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_tu:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_ta:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_tama:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_f_xu_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_f_xu_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_tu:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_ta:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_tama:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_x_f_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_rtz_x_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_tu:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_ta:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_tama:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_rtz_xu_f_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_rtz_xu_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_tu:
  ID = Intrinsic::riscv_vfwcvt_x_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_ta:
  ID = Intrinsic::riscv_vfwcvt_x_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_x_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_tama:
  ID = Intrinsic::riscv_vfwcvt_x_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_x_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_x_f_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_x_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_tu:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_ta:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_tuma:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_tama:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_tumu:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwcvt_xu_f_v_tamu:
  ID = Intrinsic::riscv_vfwcvt_xu_f_v_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_tu:
  ID = Intrinsic::riscv_vfwmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_ta:
  ID = Intrinsic::riscv_vfwmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_tuma:
  ID = Intrinsic::riscv_vfwmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_tama:
  ID = Intrinsic::riscv_vfwmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_tumu:
  ID = Intrinsic::riscv_vfwmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwmacc_vf_tamu:
  ID = Intrinsic::riscv_vfwmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_tu:
  ID = Intrinsic::riscv_vfwmsac;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_ta:
  ID = Intrinsic::riscv_vfwmsac;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_tuma:
  ID = Intrinsic::riscv_vfwmsac_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_tama:
  ID = Intrinsic::riscv_vfwmsac_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_tumu:
  ID = Intrinsic::riscv_vfwmsac_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmsac_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwmsac_vf_tamu:
  ID = Intrinsic::riscv_vfwmsac_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_tu:
  ID = Intrinsic::riscv_vfwmul;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_ta:
  ID = Intrinsic::riscv_vfwmul;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_tuma:
  ID = Intrinsic::riscv_vfwmul_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_tama:
  ID = Intrinsic::riscv_vfwmul_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_tumu:
  ID = Intrinsic::riscv_vfwmul_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwmul_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwmul_vf_tamu:
  ID = Intrinsic::riscv_vfwmul_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_tu:
  ID = Intrinsic::riscv_vfwnmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_ta:
  ID = Intrinsic::riscv_vfwnmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_tuma:
  ID = Intrinsic::riscv_vfwnmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_tama:
  ID = Intrinsic::riscv_vfwnmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_tumu:
  ID = Intrinsic::riscv_vfwnmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwnmacc_vf_tamu:
  ID = Intrinsic::riscv_vfwnmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_tu:
  ID = Intrinsic::riscv_vfwnmsac;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_ta:
  ID = Intrinsic::riscv_vfwnmsac;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_tuma:
  ID = Intrinsic::riscv_vfwnmsac_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_tama:
  ID = Intrinsic::riscv_vfwnmsac_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_tumu:
  ID = Intrinsic::riscv_vfwnmsac_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwnmsac_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwnmsac_vf_tamu:
  ID = Intrinsic::riscv_vfwnmsac_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredosum_vs_tu:
  ID = Intrinsic::riscv_vfwredosum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredosum_vs_ta:
  ID = Intrinsic::riscv_vfwredosum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredosum_vs_tum:
  ID = Intrinsic::riscv_vfwredosum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredosum_vs_tama:
case RISCVVector::BI__builtin_rvv_vfwredosum_vs_tam:
  ID = Intrinsic::riscv_vfwredosum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredusum_vs_tu:
  ID = Intrinsic::riscv_vfwredusum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredusum_vs_ta:
  ID = Intrinsic::riscv_vfwredusum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredusum_vs_tum:
  ID = Intrinsic::riscv_vfwredusum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwredusum_vs_tama:
case RISCVVector::BI__builtin_rvv_vfwredusum_vs_tam:
  ID = Intrinsic::riscv_vfwredusum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_tu:
  ID = Intrinsic::riscv_vfwsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_ta:
  ID = Intrinsic::riscv_vfwsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_tuma:
  ID = Intrinsic::riscv_vfwsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_tama:
  ID = Intrinsic::riscv_vfwsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_tumu:
  ID = Intrinsic::riscv_vfwsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vfwsub_vf_tamu:
  ID = Intrinsic::riscv_vfwsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_tu:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_tu:
  ID = Intrinsic::riscv_vfwsub_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_ta:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_ta:
  ID = Intrinsic::riscv_vfwsub_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_tuma:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_tuma:
  ID = Intrinsic::riscv_vfwsub_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_tama:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_tama:
  ID = Intrinsic::riscv_vfwsub_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_tumu:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_tumu:
  ID = Intrinsic::riscv_vfwsub_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vfwsub_wv_tamu:
case RISCVVector::BI__builtin_rvv_vfwsub_wf_tamu:
  ID = Intrinsic::riscv_vfwsub_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_tu:
  ID = Intrinsic::riscv_vid;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_ta:
  ID = Intrinsic::riscv_vid;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_tuma:
  ID = Intrinsic::riscv_vid_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_tama:
  ID = Intrinsic::riscv_vid_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_tumu:
  ID = Intrinsic::riscv_vid_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vid_v_tamu:
  ID = Intrinsic::riscv_vid_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_tu:
  ID = Intrinsic::riscv_viota;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_ta:
  ID = Intrinsic::riscv_viota;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_tuma:
  ID = Intrinsic::riscv_viota_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_tama:
  ID = Intrinsic::riscv_viota_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_tumu:
  ID = Intrinsic::riscv_viota_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_viota_m_tamu:
  ID = Intrinsic::riscv_viota_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_tu:
case RISCVVector::BI__builtin_rvv_vle16_v_tu:
case RISCVVector::BI__builtin_rvv_vle32_v_tu:
case RISCVVector::BI__builtin_rvv_vle64_v_tu:
  ID = Intrinsic::riscv_vle;
  PolicyAttrs = 2;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_ta:
case RISCVVector::BI__builtin_rvv_vle16_v_ta:
case RISCVVector::BI__builtin_rvv_vle32_v_ta:
case RISCVVector::BI__builtin_rvv_vle64_v_ta:
  ID = Intrinsic::riscv_vle;
  PolicyAttrs = 3;
  Ops[0] = Builder.CreateBitCast(Ops[0], ResultType->getPointerTo());
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_tuma:
case RISCVVector::BI__builtin_rvv_vle16_v_tuma:
case RISCVVector::BI__builtin_rvv_vle32_v_tuma:
case RISCVVector::BI__builtin_rvv_vle64_v_tuma:
  ID = Intrinsic::riscv_vle_mask;
  PolicyAttrs = 2;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_tama:
case RISCVVector::BI__builtin_rvv_vle16_v_tama:
case RISCVVector::BI__builtin_rvv_vle32_v_tama:
case RISCVVector::BI__builtin_rvv_vle64_v_tama:
  ID = Intrinsic::riscv_vle_mask;
  PolicyAttrs = 3;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_tumu:
case RISCVVector::BI__builtin_rvv_vle16_v_tumu:
case RISCVVector::BI__builtin_rvv_vle32_v_tumu:
case RISCVVector::BI__builtin_rvv_vle64_v_tumu:
  ID = Intrinsic::riscv_vle_mask;
  PolicyAttrs = 0;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle8_v_tamu:
case RISCVVector::BI__builtin_rvv_vle16_v_tamu:
case RISCVVector::BI__builtin_rvv_vle32_v_tamu:
case RISCVVector::BI__builtin_rvv_vle64_v_tamu:
  ID = Intrinsic::riscv_vle_mask;
  PolicyAttrs = 1;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vle32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vle64ff_v_tu:
case RISCVVector::BI__builtin_rvv_vle8ff_v_tu:
  ID = Intrinsic::riscv_vleff;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vle32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vle64ff_v_ta:
case RISCVVector::BI__builtin_rvv_vle8ff_v_ta:
  ID = Intrinsic::riscv_vleff;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vle32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vle64ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vle8ff_v_tuma:
  ID = Intrinsic::riscv_vleff_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vle32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vle64ff_v_tama:
case RISCVVector::BI__builtin_rvv_vle8ff_v_tama:
  ID = Intrinsic::riscv_vleff_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vle32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vle64ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vle8ff_v_tumu:
  ID = Intrinsic::riscv_vleff_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vle16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vle32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vle64ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vle8ff_v_tamu:
  ID = Intrinsic::riscv_vleff_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          // Move mask to right before vl.
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          IntrinsicTypes = {ResultType, Ops[4]->getType()};
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
          IntrinsicTypes = {ResultType, Ops[3]->getType()};
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
        Value *NewVL = Ops[2];
        Ops.erase(Ops.begin() + 2);
        llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
        llvm::Value *LoadValue = Builder.CreateCall(F, Ops, "");
        llvm::Value *V = Builder.CreateExtractValue(LoadValue, {0});
        // Store new_vl.
        clang::CharUnits Align;
        if (IsMasked)
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(E->getNumArgs()-2)->getType());
        else
          Align = CGM.getNaturalPointeeTypeAlignment(E->getArg(1)->getType());
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {1});
        Builder.CreateStore(Val, Address(NewVL, Val->getType(), Align));
        return V;
      }
      break;
case RISCVVector::BI__builtin_rvv_vlm_v:
  ID = Intrinsic::riscv_vlm;
  PolicyAttrs = 3;
  Ops[0] = Builder.CreateBitCast(Ops[0], ResultType->getPointerTo());
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxei64_v_tu:
  ID = Intrinsic::riscv_vloxei;
  PolicyAttrs = 2;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxei64_v_ta:
  ID = Intrinsic::riscv_vloxei;
  PolicyAttrs = 3;
  Ops[0] = Builder.CreateBitCast(Ops[0], ResultType->getPointerTo());
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxei64_v_tuma:
  ID = Intrinsic::riscv_vloxei_mask;
  PolicyAttrs = 2;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxei64_v_tama:
  ID = Intrinsic::riscv_vloxei_mask;
  PolicyAttrs = 3;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxei64_v_tumu:
  ID = Intrinsic::riscv_vloxei_mask;
  PolicyAttrs = 0;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxei64_v_tamu:
  ID = Intrinsic::riscv_vloxei_mask;
  PolicyAttrs = 1;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_tu:
  ID = Intrinsic::riscv_vloxseg2;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_ta:
  ID = Intrinsic::riscv_vloxseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_tuma:
  ID = Intrinsic::riscv_vloxseg2_mask;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_tama:
  ID = Intrinsic::riscv_vloxseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_tumu:
  ID = Intrinsic::riscv_vloxseg2_mask;
  NF = 2;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg2ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei64_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg2ei8_v_tamu:
  ID = Intrinsic::riscv_vloxseg2_mask;
  NF = 2;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg3;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg3_mask;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg3_mask;
  NF = 3;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg3ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg3ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg3_mask;
  NF = 3;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg4;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg4_mask;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg4_mask;
  NF = 4;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg4ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg4ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg4_mask;
  NF = 4;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg5;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg5_mask;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg5_mask;
  NF = 5;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg5ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg5ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg5_mask;
  NF = 5;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg6;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg6_mask;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg6_mask;
  NF = 6;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg6ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg6ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg6_mask;
  NF = 6;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg7;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg7_mask;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg7_mask;
  NF = 7;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg7ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg7ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg7_mask;
  NF = 7;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_tu:
  ID = Intrinsic::riscv_vloxseg8;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_ta:
  ID = Intrinsic::riscv_vloxseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_tuma:
  ID = Intrinsic::riscv_vloxseg8_mask;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_tama:
  ID = Intrinsic::riscv_vloxseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_tumu:
  ID = Intrinsic::riscv_vloxseg8_mask;
  NF = 8;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vloxseg8ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vloxseg8ei64_v_tamu:
  ID = Intrinsic::riscv_vloxseg8_mask;
  NF = 8;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlse32_v_tu:
case RISCVVector::BI__builtin_rvv_vlse64_v_tu:
case RISCVVector::BI__builtin_rvv_vlse8_v_tu:
case RISCVVector::BI__builtin_rvv_vlse16_v_tu:
  ID = Intrinsic::riscv_vlse;
  PolicyAttrs = 2;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlse32_v_ta:
case RISCVVector::BI__builtin_rvv_vlse64_v_ta:
case RISCVVector::BI__builtin_rvv_vlse8_v_ta:
case RISCVVector::BI__builtin_rvv_vlse16_v_ta:
  ID = Intrinsic::riscv_vlse;
  PolicyAttrs = 3;
  Ops[0] = Builder.CreateBitCast(Ops[0], ResultType->getPointerTo());
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlse32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlse64_v_tuma:
case RISCVVector::BI__builtin_rvv_vlse8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlse16_v_tuma:
  ID = Intrinsic::riscv_vlse_mask;
  PolicyAttrs = 2;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlse32_v_tama:
case RISCVVector::BI__builtin_rvv_vlse64_v_tama:
case RISCVVector::BI__builtin_rvv_vlse8_v_tama:
case RISCVVector::BI__builtin_rvv_vlse16_v_tama:
  ID = Intrinsic::riscv_vlse_mask;
  PolicyAttrs = 3;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlse32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlse64_v_tumu:
case RISCVVector::BI__builtin_rvv_vlse8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlse16_v_tumu:
  ID = Intrinsic::riscv_vlse_mask;
  PolicyAttrs = 0;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlse32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlse64_v_tamu:
case RISCVVector::BI__builtin_rvv_vlse8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlse16_v_tamu:
  ID = Intrinsic::riscv_vlse_mask;
  PolicyAttrs = 1;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_tu:
  ID = Intrinsic::riscv_vlseg2;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_ta:
  ID = Intrinsic::riscv_vlseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_tuma:
  ID = Intrinsic::riscv_vlseg2_mask;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_tama:
  ID = Intrinsic::riscv_vlseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_tumu:
  ID = Intrinsic::riscv_vlseg2_mask;
  NF = 2;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e64_v_tamu:
  ID = Intrinsic::riscv_vlseg2_mask;
  NF = 2;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_tu:
  ID = Intrinsic::riscv_vlseg2ff;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_ta:
  ID = Intrinsic::riscv_vlseg2ff;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_tuma:
  ID = Intrinsic::riscv_vlseg2ff_mask;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_tama:
  ID = Intrinsic::riscv_vlseg2ff_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_tumu:
  ID = Intrinsic::riscv_vlseg2ff_mask;
  NF = 2;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg2e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e64ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg2e8ff_v_tamu:
  ID = Intrinsic::riscv_vlseg2ff_mask;
  NF = 2;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_tu:
  ID = Intrinsic::riscv_vlseg3;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_ta:
  ID = Intrinsic::riscv_vlseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_tuma:
  ID = Intrinsic::riscv_vlseg3_mask;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_tama:
  ID = Intrinsic::riscv_vlseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_tumu:
  ID = Intrinsic::riscv_vlseg3_mask;
  NF = 3;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e64_v_tamu:
  ID = Intrinsic::riscv_vlseg3_mask;
  NF = 3;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg3ff;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg3ff;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg3ff_mask;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg3ff_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg3ff_mask;
  NF = 3;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg3e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg3e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg3ff_mask;
  NF = 3;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_tu:
  ID = Intrinsic::riscv_vlseg4;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_ta:
  ID = Intrinsic::riscv_vlseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_tuma:
  ID = Intrinsic::riscv_vlseg4_mask;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_tama:
  ID = Intrinsic::riscv_vlseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_tumu:
  ID = Intrinsic::riscv_vlseg4_mask;
  NF = 4;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e64_v_tamu:
  ID = Intrinsic::riscv_vlseg4_mask;
  NF = 4;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg4ff;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg4ff;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg4ff_mask;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg4ff_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg4ff_mask;
  NF = 4;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg4e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg4e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg4ff_mask;
  NF = 4;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_tu:
  ID = Intrinsic::riscv_vlseg5;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_ta:
  ID = Intrinsic::riscv_vlseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_tuma:
  ID = Intrinsic::riscv_vlseg5_mask;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_tama:
  ID = Intrinsic::riscv_vlseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_tumu:
  ID = Intrinsic::riscv_vlseg5_mask;
  NF = 5;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e64_v_tamu:
  ID = Intrinsic::riscv_vlseg5_mask;
  NF = 5;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg5ff;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg5ff;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg5ff_mask;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg5ff_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg5ff_mask;
  NF = 5;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg5e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg5e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg5ff_mask;
  NF = 5;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_tu:
  ID = Intrinsic::riscv_vlseg6;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_ta:
  ID = Intrinsic::riscv_vlseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_tuma:
  ID = Intrinsic::riscv_vlseg6_mask;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_tama:
  ID = Intrinsic::riscv_vlseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_tumu:
  ID = Intrinsic::riscv_vlseg6_mask;
  NF = 6;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e64_v_tamu:
  ID = Intrinsic::riscv_vlseg6_mask;
  NF = 6;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg6ff;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg6ff;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg6ff_mask;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg6ff_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg6ff_mask;
  NF = 6;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg6e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg6e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg6ff_mask;
  NF = 6;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_tu:
  ID = Intrinsic::riscv_vlseg7;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_ta:
  ID = Intrinsic::riscv_vlseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_tuma:
  ID = Intrinsic::riscv_vlseg7_mask;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_tama:
  ID = Intrinsic::riscv_vlseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_tumu:
  ID = Intrinsic::riscv_vlseg7_mask;
  NF = 7;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e64_v_tamu:
  ID = Intrinsic::riscv_vlseg7_mask;
  NF = 7;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg7ff;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg7ff;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg7ff_mask;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg7ff_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg7ff_mask;
  NF = 7;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg7e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg7e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg7ff_mask;
  NF = 7;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_tu:
  ID = Intrinsic::riscv_vlseg8;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_ta:
  ID = Intrinsic::riscv_vlseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_tuma:
  ID = Intrinsic::riscv_vlseg8_mask;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_tama:
  ID = Intrinsic::riscv_vlseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_tumu:
  ID = Intrinsic::riscv_vlseg8_mask;
  NF = 8;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e64_v_tamu:
  ID = Intrinsic::riscv_vlseg8_mask;
  NF = 8;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *VLOperand = Ops[PtrOperandIdx + 1];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_tu:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_tu:
  ID = Intrinsic::riscv_vlseg8ff;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_ta:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_ta:
  ID = Intrinsic::riscv_vlseg8ff;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_tuma:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_tuma:
  ID = Intrinsic::riscv_vlseg8ff_mask;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_tama:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_tama:
  ID = Intrinsic::riscv_vlseg8ff_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_tumu:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_tumu:
  ID = Intrinsic::riscv_vlseg8ff_mask;
  NF = 8;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlseg8e8ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e16ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e32ff_v_tamu:
case RISCVVector::BI__builtin_rvv_vlseg8e64ff_v_tamu:
  ID = Intrinsic::riscv_vlseg8ff_mask;
  NF = 8;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *NewVLOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      // Store new_vl.
      llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {NF});
      return Builder.CreateStore(Val, Address(NewVLOperand, Val->getType(), Align));
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_tu:
  ID = Intrinsic::riscv_vlsseg2;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_ta:
  ID = Intrinsic::riscv_vlsseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg2_mask;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_tama:
  ID = Intrinsic::riscv_vlsseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg2_mask;
  NF = 2;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg2e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg2e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg2e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg2e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg2_mask;
  NF = 2;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_tu:
  ID = Intrinsic::riscv_vlsseg3;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_ta:
  ID = Intrinsic::riscv_vlsseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg3_mask;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_tama:
  ID = Intrinsic::riscv_vlsseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg3_mask;
  NF = 3;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg3e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg3e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg3e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg3e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg3_mask;
  NF = 3;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_tu:
  ID = Intrinsic::riscv_vlsseg4;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_ta:
  ID = Intrinsic::riscv_vlsseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg4_mask;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_tama:
  ID = Intrinsic::riscv_vlsseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg4_mask;
  NF = 4;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg4e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg4e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg4e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg4e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg4_mask;
  NF = 4;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_tu:
  ID = Intrinsic::riscv_vlsseg5;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_ta:
  ID = Intrinsic::riscv_vlsseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg5_mask;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_tama:
  ID = Intrinsic::riscv_vlsseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg5_mask;
  NF = 5;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg5e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg5e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg5e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg5e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg5_mask;
  NF = 5;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_tu:
  ID = Intrinsic::riscv_vlsseg6;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_ta:
  ID = Intrinsic::riscv_vlsseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg6_mask;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_tama:
  ID = Intrinsic::riscv_vlsseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg6_mask;
  NF = 6;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg6e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg6e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg6e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg6e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg6_mask;
  NF = 6;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_tu:
  ID = Intrinsic::riscv_vlsseg7;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_ta:
  ID = Intrinsic::riscv_vlsseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg7_mask;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_tama:
  ID = Intrinsic::riscv_vlsseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg7_mask;
  NF = 7;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg7e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg7e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg7e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg7e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg7_mask;
  NF = 7;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_tu:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_tu:
  ID = Intrinsic::riscv_vlsseg8;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_ta:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_ta:
  ID = Intrinsic::riscv_vlsseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_tuma:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_tuma:
  ID = Intrinsic::riscv_vlsseg8_mask;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_tama:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_tama:
  ID = Intrinsic::riscv_vlsseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_tumu:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_tumu:
  ID = Intrinsic::riscv_vlsseg8_mask;
  NF = 8;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vlsseg8e8_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg8e16_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg8e32_v_tamu:
case RISCVVector::BI__builtin_rvv_vlsseg8e64_v_tamu:
  ID = Intrinsic::riscv_vlsseg8_mask;
  NF = 8;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *StrideOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(StrideOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxei64_v_tu:
case RISCVVector::BI__builtin_rvv_vluxei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxei32_v_tu:
  ID = Intrinsic::riscv_vluxei;
  PolicyAttrs = 2;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxei64_v_ta:
case RISCVVector::BI__builtin_rvv_vluxei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxei32_v_ta:
  ID = Intrinsic::riscv_vluxei;
  PolicyAttrs = 3;
  Ops[0] = Builder.CreateBitCast(Ops[0], ResultType->getPointerTo());
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxei64_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxei32_v_tuma:
  ID = Intrinsic::riscv_vluxei_mask;
  PolicyAttrs = 2;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxei64_v_tama:
case RISCVVector::BI__builtin_rvv_vluxei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxei32_v_tama:
  ID = Intrinsic::riscv_vluxei_mask;
  PolicyAttrs = 3;
  Ops[1] = Builder.CreateBitCast(Ops[1], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxei64_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxei32_v_tumu:
  ID = Intrinsic::riscv_vluxei_mask;
  PolicyAttrs = 0;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxei64_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxei32_v_tamu:
  ID = Intrinsic::riscv_vluxei_mask;
  PolicyAttrs = 1;
  Ops[2] = Builder.CreateBitCast(Ops[2], ResultType->getPointerTo());
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg2;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg2_mask;
  NF = 2;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg2_mask;
  NF = 2;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg2ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg2ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg2_mask;
  NF = 2;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg3;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg3_mask;
  NF = 3;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg3_mask;
  NF = 3;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg3ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg3ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg3_mask;
  NF = 3;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg4;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg4_mask;
  NF = 4;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg4_mask;
  NF = 4;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg4ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg4ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg4_mask;
  NF = 4;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg5;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg5_mask;
  NF = 5;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg5_mask;
  NF = 5;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg5ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg5ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg5_mask;
  NF = 5;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg6;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg6_mask;
  NF = 6;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg6_mask;
  NF = 6;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg6ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg6ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg6_mask;
  NF = 6;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg7;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg7_mask;
  NF = 7;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg7_mask;
  NF = 7;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg7ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg7ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg7_mask;
  NF = 7;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_tu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_tu:
  ID = Intrinsic::riscv_vluxseg8;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_ta:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_ta:
  ID = Intrinsic::riscv_vluxseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_tuma:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_tuma:
  ID = Intrinsic::riscv_vluxseg8_mask;
  NF = 8;
  PolicyAttrs = 2;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_tama:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_tama:
  ID = Intrinsic::riscv_vluxseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_tumu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_tumu:
  ID = Intrinsic::riscv_vluxseg8_mask;
  NF = 8;
  PolicyAttrs = 0;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vluxseg8ei8_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei16_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei32_v_tamu:
case RISCVVector::BI__builtin_rvv_vluxseg8ei64_v_tamu:
  ID = Intrinsic::riscv_vluxseg8_mask;
  NF = 8;
  PolicyAttrs = 1;
IsMasked = true;

    {
      ResultType = ConvertType(E->getArg(0)->getType()->getPointeeType());
      SmallVector<llvm::Value*, 12> Operands;

      // Please refer to comment under 'defvar NFList' in this file
      if ((IsMasked && (PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ||
          (!IsMasked && PolicyAttrs & RVV_VTA))
        Operands.append(NF, llvm::PoisonValue::get(ResultType));
      else {
        if (IsMasked)
          Operands.append(Ops.begin() + NF + 1, Ops.begin() + 2 * NF + 1);
        else // Unmasked
          Operands.append(Ops.begin() + NF, Ops.begin() + 2 * NF);
      }
      unsigned PtrOperandIdx = IsMasked ?
        ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA)) ? NF + 1 : 2 * NF + 1 :
        (PolicyAttrs & RVV_VTA) ? NF : 2 * NF;
      Value *PtrOperand = Ops[PtrOperandIdx];
      Value *IndexOperand = Ops[PtrOperandIdx + 1];
      Value *VLOperand = Ops[PtrOperandIdx + 2];
      Operands.push_back(PtrOperand);
      Operands.push_back(IndexOperand);
      if (IsMasked)
        Operands.push_back(Ops[NF]);
      Operands.push_back(VLOperand);
      if (IsMasked)
        Operands.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, IndexOperand->getType(), Ops.back()->getType()};

      llvm::Function *F = CGM.getIntrinsic(ID, IntrinsicTypes);
      llvm::Value *LoadValue = Builder.CreateCall(F, Operands, "");
      clang::CharUnits Align =
          CGM.getNaturalPointeeTypeAlignment(E->getArg(0)->getType());
      llvm::Value *V;
      for (unsigned I = 0; I < NF; ++I) {
        llvm::Value *Val = Builder.CreateExtractValue(LoadValue, {I});
        V = Builder.CreateStore(Val, Address(Ops[I], Val->getType(), Align));
      }
      return V;
    }
    break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vmacc_vx_tu:
  ID = Intrinsic::riscv_vmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vmacc_vx_ta:
  ID = Intrinsic::riscv_vmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmacc_vx_tuma:
  ID = Intrinsic::riscv_vmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vmacc_vx_tama:
  ID = Intrinsic::riscv_vmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmacc_vx_tumu:
  ID = Intrinsic::riscv_vmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmacc_vx_tamu:
  ID = Intrinsic::riscv_vmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadc_vv:
case RISCVVector::BI__builtin_rvv_vmadc_vx:
  ID = Intrinsic::riscv_vmadc;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadc_vvm:
case RISCVVector::BI__builtin_rvv_vmadc_vxm:
  ID = Intrinsic::riscv_vmadc_carry_in;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vmadd_vx_tu:
  ID = Intrinsic::riscv_vmadd;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vmadd_vx_ta:
  ID = Intrinsic::riscv_vmadd;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmadd_vx_tuma:
  ID = Intrinsic::riscv_vmadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vmadd_vx_tama:
  ID = Intrinsic::riscv_vmadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmadd_vx_tumu:
  ID = Intrinsic::riscv_vmadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmadd_vx_tamu:
  ID = Intrinsic::riscv_vmadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmmv_m:
  ID = Intrinsic::riscv_vmand;
  PolicyAttrs = 3;
IsMasked = false;

      {
        // op1, vl
        IntrinsicTypes = {ResultType,
                          Ops[1]->getType()};
        Ops.insert(Ops.begin() + 1, Ops[0]);
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vmand_mm:
  ID = Intrinsic::riscv_vmand;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmandn_mm:
  ID = Intrinsic::riscv_vmandn;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_tu:
case RISCVVector::BI__builtin_rvv_vmax_vx_tu:
  ID = Intrinsic::riscv_vmax;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_ta:
case RISCVVector::BI__builtin_rvv_vmax_vx_ta:
  ID = Intrinsic::riscv_vmax;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmax_vx_tuma:
  ID = Intrinsic::riscv_vmax_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_tama:
case RISCVVector::BI__builtin_rvv_vmax_vx_tama:
  ID = Intrinsic::riscv_vmax_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmax_vx_tumu:
  ID = Intrinsic::riscv_vmax_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmax_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmax_vx_tamu:
  ID = Intrinsic::riscv_vmax_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_tu:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_tu:
  ID = Intrinsic::riscv_vmaxu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_ta:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_ta:
  ID = Intrinsic::riscv_vmaxu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_tuma:
  ID = Intrinsic::riscv_vmaxu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_tama:
  ID = Intrinsic::riscv_vmaxu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_tumu:
  ID = Intrinsic::riscv_vmaxu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmaxu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmaxu_vx_tamu:
  ID = Intrinsic::riscv_vmaxu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmclr_m:
  ID = Intrinsic::riscv_vmclr;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmerge_vvm_tu:
case RISCVVector::BI__builtin_rvv_vmerge_vxm_tu:
  ID = Intrinsic::riscv_vmerge;
  PolicyAttrs = 2;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vmerge_vvm_ta:
case RISCVVector::BI__builtin_rvv_vmerge_vxm_ta:
  ID = Intrinsic::riscv_vmerge;
  PolicyAttrs = 3;
IsMasked = false;

      // insert poison passthru
      if (PolicyAttrs & RVV_VTA)
        Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vmfeq_vv:
case RISCVVector::BI__builtin_rvv_vmfeq_vf:
  ID = Intrinsic::riscv_vmfeq;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfeq_vv_tama:
case RISCVVector::BI__builtin_rvv_vmfeq_vv_ma:
case RISCVVector::BI__builtin_rvv_vmfeq_vf_tama:
case RISCVVector::BI__builtin_rvv_vmfeq_vf_ma:
  ID = Intrinsic::riscv_vmfeq_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfeq_vv_mu:
case RISCVVector::BI__builtin_rvv_vmfeq_vf_mu:
  ID = Intrinsic::riscv_vmfeq_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfge_vv:
case RISCVVector::BI__builtin_rvv_vmfge_vf:
  ID = Intrinsic::riscv_vmfge;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfge_vv_tama:
case RISCVVector::BI__builtin_rvv_vmfge_vv_ma:
case RISCVVector::BI__builtin_rvv_vmfge_vf_tama:
case RISCVVector::BI__builtin_rvv_vmfge_vf_ma:
  ID = Intrinsic::riscv_vmfge_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfge_vv_mu:
case RISCVVector::BI__builtin_rvv_vmfge_vf_mu:
  ID = Intrinsic::riscv_vmfge_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfgt_vv:
case RISCVVector::BI__builtin_rvv_vmfgt_vf:
  ID = Intrinsic::riscv_vmfgt;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfgt_vv_tama:
case RISCVVector::BI__builtin_rvv_vmfgt_vv_ma:
case RISCVVector::BI__builtin_rvv_vmfgt_vf_tama:
case RISCVVector::BI__builtin_rvv_vmfgt_vf_ma:
  ID = Intrinsic::riscv_vmfgt_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfgt_vv_mu:
case RISCVVector::BI__builtin_rvv_vmfgt_vf_mu:
  ID = Intrinsic::riscv_vmfgt_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfle_vv:
case RISCVVector::BI__builtin_rvv_vmfle_vf:
  ID = Intrinsic::riscv_vmfle;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfle_vv_tama:
case RISCVVector::BI__builtin_rvv_vmfle_vv_ma:
case RISCVVector::BI__builtin_rvv_vmfle_vf_tama:
case RISCVVector::BI__builtin_rvv_vmfle_vf_ma:
  ID = Intrinsic::riscv_vmfle_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfle_vv_mu:
case RISCVVector::BI__builtin_rvv_vmfle_vf_mu:
  ID = Intrinsic::riscv_vmfle_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmflt_vv:
case RISCVVector::BI__builtin_rvv_vmflt_vf:
  ID = Intrinsic::riscv_vmflt;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmflt_vv_tama:
case RISCVVector::BI__builtin_rvv_vmflt_vv_ma:
case RISCVVector::BI__builtin_rvv_vmflt_vf_tama:
case RISCVVector::BI__builtin_rvv_vmflt_vf_ma:
  ID = Intrinsic::riscv_vmflt_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmflt_vv_mu:
case RISCVVector::BI__builtin_rvv_vmflt_vf_mu:
  ID = Intrinsic::riscv_vmflt_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfne_vv:
case RISCVVector::BI__builtin_rvv_vmfne_vf:
  ID = Intrinsic::riscv_vmfne;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfne_vv_tama:
case RISCVVector::BI__builtin_rvv_vmfne_vv_ma:
case RISCVVector::BI__builtin_rvv_vmfne_vf_tama:
case RISCVVector::BI__builtin_rvv_vmfne_vf_ma:
  ID = Intrinsic::riscv_vmfne_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmfne_vv_mu:
case RISCVVector::BI__builtin_rvv_vmfne_vf_mu:
  ID = Intrinsic::riscv_vmfne_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_tu:
case RISCVVector::BI__builtin_rvv_vmin_vx_tu:
  ID = Intrinsic::riscv_vmin;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_ta:
case RISCVVector::BI__builtin_rvv_vmin_vx_ta:
  ID = Intrinsic::riscv_vmin;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmin_vx_tuma:
  ID = Intrinsic::riscv_vmin_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_tama:
case RISCVVector::BI__builtin_rvv_vmin_vx_tama:
  ID = Intrinsic::riscv_vmin_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmin_vx_tumu:
  ID = Intrinsic::riscv_vmin_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmin_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmin_vx_tamu:
  ID = Intrinsic::riscv_vmin_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_tu:
case RISCVVector::BI__builtin_rvv_vminu_vx_tu:
  ID = Intrinsic::riscv_vminu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_ta:
case RISCVVector::BI__builtin_rvv_vminu_vx_ta:
  ID = Intrinsic::riscv_vminu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vminu_vx_tuma:
  ID = Intrinsic::riscv_vminu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_tama:
case RISCVVector::BI__builtin_rvv_vminu_vx_tama:
  ID = Intrinsic::riscv_vminu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vminu_vx_tumu:
  ID = Intrinsic::riscv_vminu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vminu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vminu_vx_tamu:
  ID = Intrinsic::riscv_vminu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmnot_m:
  ID = Intrinsic::riscv_vmnand;
  PolicyAttrs = 3;
IsMasked = false;

      {
        // op1, vl
        IntrinsicTypes = {ResultType,
                          Ops[1]->getType()};
        Ops.insert(Ops.begin() + 1, Ops[0]);
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vmnand_mm:
  ID = Intrinsic::riscv_vmnand;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmnor_mm:
  ID = Intrinsic::riscv_vmnor;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmor_mm:
  ID = Intrinsic::riscv_vmor;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmorn_mm:
  ID = Intrinsic::riscv_vmorn;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsbc_vv:
case RISCVVector::BI__builtin_rvv_vmsbc_vx:
  ID = Intrinsic::riscv_vmsbc;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsbc_vvm:
case RISCVVector::BI__builtin_rvv_vmsbc_vxm:
  ID = Intrinsic::riscv_vmsbc_borrow_in;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsbf_m:
  ID = Intrinsic::riscv_vmsbf;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsbf_m_tama:
case RISCVVector::BI__builtin_rvv_vmsbf_m_ma:
  ID = Intrinsic::riscv_vmsbf_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsbf_m_mu:
  ID = Intrinsic::riscv_vmsbf_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmseq_vv:
case RISCVVector::BI__builtin_rvv_vmseq_vx:
  ID = Intrinsic::riscv_vmseq;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmseq_vv_tama:
case RISCVVector::BI__builtin_rvv_vmseq_vv_ma:
case RISCVVector::BI__builtin_rvv_vmseq_vx_tama:
case RISCVVector::BI__builtin_rvv_vmseq_vx_ma:
  ID = Intrinsic::riscv_vmseq_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmseq_vv_mu:
case RISCVVector::BI__builtin_rvv_vmseq_vx_mu:
  ID = Intrinsic::riscv_vmseq_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmset_m:
  ID = Intrinsic::riscv_vmset;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsge_vv:
case RISCVVector::BI__builtin_rvv_vmsge_vx:
  ID = Intrinsic::riscv_vmsge;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsge_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsge_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsge_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsge_vx_ma:
  ID = Intrinsic::riscv_vmsge_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsge_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsge_vx_mu:
  ID = Intrinsic::riscv_vmsge_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgeu_vv:
case RISCVVector::BI__builtin_rvv_vmsgeu_vx:
  ID = Intrinsic::riscv_vmsgeu;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgeu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsgeu_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsgeu_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsgeu_vx_ma:
  ID = Intrinsic::riscv_vmsgeu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgeu_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsgeu_vx_mu:
  ID = Intrinsic::riscv_vmsgeu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgt_vv:
case RISCVVector::BI__builtin_rvv_vmsgt_vx:
  ID = Intrinsic::riscv_vmsgt;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgt_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsgt_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsgt_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsgt_vx_ma:
  ID = Intrinsic::riscv_vmsgt_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgt_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsgt_vx_mu:
  ID = Intrinsic::riscv_vmsgt_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgtu_vv:
case RISCVVector::BI__builtin_rvv_vmsgtu_vx:
  ID = Intrinsic::riscv_vmsgtu;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgtu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsgtu_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsgtu_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsgtu_vx_ma:
  ID = Intrinsic::riscv_vmsgtu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsgtu_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsgtu_vx_mu:
  ID = Intrinsic::riscv_vmsgtu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsif_m:
  ID = Intrinsic::riscv_vmsif;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsif_m_tama:
case RISCVVector::BI__builtin_rvv_vmsif_m_ma:
  ID = Intrinsic::riscv_vmsif_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsif_m_mu:
  ID = Intrinsic::riscv_vmsif_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsle_vv:
case RISCVVector::BI__builtin_rvv_vmsle_vx:
  ID = Intrinsic::riscv_vmsle;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsle_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsle_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsle_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsle_vx_ma:
  ID = Intrinsic::riscv_vmsle_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsle_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsle_vx_mu:
  ID = Intrinsic::riscv_vmsle_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsleu_vv:
case RISCVVector::BI__builtin_rvv_vmsleu_vx:
  ID = Intrinsic::riscv_vmsleu;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsleu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsleu_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsleu_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsleu_vx_ma:
  ID = Intrinsic::riscv_vmsleu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsleu_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsleu_vx_mu:
  ID = Intrinsic::riscv_vmsleu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmslt_vv:
case RISCVVector::BI__builtin_rvv_vmslt_vx:
  ID = Intrinsic::riscv_vmslt;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmslt_vv_tama:
case RISCVVector::BI__builtin_rvv_vmslt_vv_ma:
case RISCVVector::BI__builtin_rvv_vmslt_vx_tama:
case RISCVVector::BI__builtin_rvv_vmslt_vx_ma:
  ID = Intrinsic::riscv_vmslt_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmslt_vv_mu:
case RISCVVector::BI__builtin_rvv_vmslt_vx_mu:
  ID = Intrinsic::riscv_vmslt_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsltu_vv:
case RISCVVector::BI__builtin_rvv_vmsltu_vx:
  ID = Intrinsic::riscv_vmsltu;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsltu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsltu_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsltu_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsltu_vx_ma:
  ID = Intrinsic::riscv_vmsltu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsltu_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsltu_vx_mu:
  ID = Intrinsic::riscv_vmsltu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsne_vv:
case RISCVVector::BI__builtin_rvv_vmsne_vx:
  ID = Intrinsic::riscv_vmsne;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType(), Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsne_vv_tama:
case RISCVVector::BI__builtin_rvv_vmsne_vv_ma:
case RISCVVector::BI__builtin_rvv_vmsne_vx_tama:
case RISCVVector::BI__builtin_rvv_vmsne_vx_ma:
  ID = Intrinsic::riscv_vmsne_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsne_vv_mu:
case RISCVVector::BI__builtin_rvv_vmsne_vx_mu:
  ID = Intrinsic::riscv_vmsne_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsof_m:
  ID = Intrinsic::riscv_vmsof;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsof_m_tama:
case RISCVVector::BI__builtin_rvv_vmsof_m_ma:
  ID = Intrinsic::riscv_vmsof_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmsof_m_mu:
  ID = Intrinsic::riscv_vmsof_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_tu:
case RISCVVector::BI__builtin_rvv_vmul_vx_tu:
  ID = Intrinsic::riscv_vmul;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_ta:
case RISCVVector::BI__builtin_rvv_vmul_vx_ta:
  ID = Intrinsic::riscv_vmul;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmul_vx_tuma:
  ID = Intrinsic::riscv_vmul_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_tama:
case RISCVVector::BI__builtin_rvv_vmul_vx_tama:
  ID = Intrinsic::riscv_vmul_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmul_vx_tumu:
  ID = Intrinsic::riscv_vmul_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmul_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmul_vx_tamu:
  ID = Intrinsic::riscv_vmul_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_tu:
case RISCVVector::BI__builtin_rvv_vmulh_vx_tu:
  ID = Intrinsic::riscv_vmulh;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_ta:
case RISCVVector::BI__builtin_rvv_vmulh_vx_ta:
  ID = Intrinsic::riscv_vmulh;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmulh_vx_tuma:
  ID = Intrinsic::riscv_vmulh_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_tama:
case RISCVVector::BI__builtin_rvv_vmulh_vx_tama:
  ID = Intrinsic::riscv_vmulh_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmulh_vx_tumu:
  ID = Intrinsic::riscv_vmulh_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulh_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmulh_vx_tamu:
  ID = Intrinsic::riscv_vmulh_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tu:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tu:
  ID = Intrinsic::riscv_vmulhsu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_ta:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_ta:
  ID = Intrinsic::riscv_vmulhsu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tuma:
  ID = Intrinsic::riscv_vmulhsu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tama:
  ID = Intrinsic::riscv_vmulhsu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tumu:
  ID = Intrinsic::riscv_vmulhsu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhsu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmulhsu_vx_tamu:
  ID = Intrinsic::riscv_vmulhsu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_tu:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_tu:
  ID = Intrinsic::riscv_vmulhu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_ta:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_ta:
  ID = Intrinsic::riscv_vmulhu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_tuma:
  ID = Intrinsic::riscv_vmulhu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_tama:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_tama:
  ID = Intrinsic::riscv_vmulhu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_tumu:
  ID = Intrinsic::riscv_vmulhu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmulhu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vmulhu_vx_tamu:
  ID = Intrinsic::riscv_vmulhu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_s_x_tu:
  ID = Intrinsic::riscv_vmv_s_x;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_s_x_ta:
  ID = Intrinsic::riscv_vmv_s_x;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_v_v_tu:
  ID = Intrinsic::riscv_vmv_v_v;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_v_v_ta:
  ID = Intrinsic::riscv_vmv_v_v;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_v_x_tu:
  ID = Intrinsic::riscv_vmv_v_x;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_v_x_ta:
  ID = Intrinsic::riscv_vmv_v_x;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmv_x_s:
  ID = Intrinsic::riscv_vmv_x_s;
  PolicyAttrs = 3;
  IntrinsicTypes = {Ops[0]->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmxnor_mm:
  ID = Intrinsic::riscv_vmxnor;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vmxor_mm:
  ID = Intrinsic::riscv_vmxor;
  PolicyAttrs = 3;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_tu:
case RISCVVector::BI__builtin_rvv_vnclip_wx_tu:
  ID = Intrinsic::riscv_vnclip;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_ta:
case RISCVVector::BI__builtin_rvv_vnclip_wx_ta:
  ID = Intrinsic::riscv_vnclip;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_tuma:
case RISCVVector::BI__builtin_rvv_vnclip_wx_tuma:
  ID = Intrinsic::riscv_vnclip_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_tama:
case RISCVVector::BI__builtin_rvv_vnclip_wx_tama:
  ID = Intrinsic::riscv_vnclip_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_tumu:
case RISCVVector::BI__builtin_rvv_vnclip_wx_tumu:
  ID = Intrinsic::riscv_vnclip_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclip_wv_tamu:
case RISCVVector::BI__builtin_rvv_vnclip_wx_tamu:
  ID = Intrinsic::riscv_vnclip_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_tu:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_tu:
  ID = Intrinsic::riscv_vnclipu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_ta:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_ta:
  ID = Intrinsic::riscv_vnclipu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_tuma:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_tuma:
  ID = Intrinsic::riscv_vnclipu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_tama:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_tama:
  ID = Intrinsic::riscv_vnclipu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_tumu:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_tumu:
  ID = Intrinsic::riscv_vnclipu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnclipu_wv_tamu:
case RISCVVector::BI__builtin_rvv_vnclipu_wx_tamu:
  ID = Intrinsic::riscv_vnclipu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_tu:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_tu:
  ID = Intrinsic::riscv_vnmsac;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_ta:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_ta:
  ID = Intrinsic::riscv_vnmsac;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_tuma:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_tuma:
  ID = Intrinsic::riscv_vnmsac_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_tama:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_tama:
  ID = Intrinsic::riscv_vnmsac_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_tumu:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_tumu:
  ID = Intrinsic::riscv_vnmsac_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsac_vv_tamu:
case RISCVVector::BI__builtin_rvv_vnmsac_vx_tamu:
  ID = Intrinsic::riscv_vnmsac_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_tu:
  ID = Intrinsic::riscv_vnmsub;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_ta:
  ID = Intrinsic::riscv_vnmsub;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_tuma:
  ID = Intrinsic::riscv_vnmsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_tama:
  ID = Intrinsic::riscv_vnmsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_tumu:
  ID = Intrinsic::riscv_vnmsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnmsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vnmsub_vx_tamu:
  ID = Intrinsic::riscv_vnmsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_tu:
case RISCVVector::BI__builtin_rvv_vnsra_wx_tu:
  ID = Intrinsic::riscv_vnsra;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_ta:
case RISCVVector::BI__builtin_rvv_vnsra_wx_ta:
  ID = Intrinsic::riscv_vnsra;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_tuma:
case RISCVVector::BI__builtin_rvv_vnsra_wx_tuma:
  ID = Intrinsic::riscv_vnsra_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_tama:
case RISCVVector::BI__builtin_rvv_vnsra_wx_tama:
  ID = Intrinsic::riscv_vnsra_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_tumu:
case RISCVVector::BI__builtin_rvv_vnsra_wx_tumu:
  ID = Intrinsic::riscv_vnsra_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsra_wv_tamu:
case RISCVVector::BI__builtin_rvv_vnsra_wx_tamu:
  ID = Intrinsic::riscv_vnsra_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_tu:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_tu:
  ID = Intrinsic::riscv_vnsrl;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tu:
  ID = Intrinsic::riscv_vnsrl;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_ta:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_ta:
  ID = Intrinsic::riscv_vnsrl;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_ta:
  ID = Intrinsic::riscv_vnsrl;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_tuma:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_tuma:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tuma:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_tama:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_tama:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tama:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_tumu:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_tumu:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tumu:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vnsrl_wv_tamu:
case RISCVVector::BI__builtin_rvv_vnsrl_wx_tamu:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vncvt_x_x_w_tamu:
  ID = Intrinsic::riscv_vnsrl_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(Ops.back()->getType())); 
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, xlen, mask, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            Ops[4]->getType(),
                            Ops[4]->getType()};
        } else {
          // passthru, op1, xlen, vl
          IntrinsicTypes = {ResultType,
                  Ops[1]->getType(),
                  Ops[3]->getType(),
                  Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vor_vv_tu:
case RISCVVector::BI__builtin_rvv_vor_vx_tu:
  ID = Intrinsic::riscv_vor;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vor_vv_ta:
case RISCVVector::BI__builtin_rvv_vor_vx_ta:
  ID = Intrinsic::riscv_vor;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vor_vv_tuma:
case RISCVVector::BI__builtin_rvv_vor_vx_tuma:
  ID = Intrinsic::riscv_vor_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vor_vv_tama:
case RISCVVector::BI__builtin_rvv_vor_vx_tama:
  ID = Intrinsic::riscv_vor_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vor_vv_tumu:
case RISCVVector::BI__builtin_rvv_vor_vx_tumu:
  ID = Intrinsic::riscv_vor_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vor_vv_tamu:
case RISCVVector::BI__builtin_rvv_vor_vx_tamu:
  ID = Intrinsic::riscv_vor_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredand_vs_tu:
  ID = Intrinsic::riscv_vredand;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredand_vs_ta:
  ID = Intrinsic::riscv_vredand;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredand_vs_tum:
  ID = Intrinsic::riscv_vredand_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredand_vs_tama:
case RISCVVector::BI__builtin_rvv_vredand_vs_tam:
  ID = Intrinsic::riscv_vredand_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmax_vs_tu:
  ID = Intrinsic::riscv_vredmax;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmax_vs_ta:
  ID = Intrinsic::riscv_vredmax;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmax_vs_tum:
  ID = Intrinsic::riscv_vredmax_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmax_vs_tama:
case RISCVVector::BI__builtin_rvv_vredmax_vs_tam:
  ID = Intrinsic::riscv_vredmax_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmaxu_vs_tu:
  ID = Intrinsic::riscv_vredmaxu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmaxu_vs_ta:
  ID = Intrinsic::riscv_vredmaxu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmaxu_vs_tum:
  ID = Intrinsic::riscv_vredmaxu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmaxu_vs_tama:
case RISCVVector::BI__builtin_rvv_vredmaxu_vs_tam:
  ID = Intrinsic::riscv_vredmaxu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmin_vs_tu:
  ID = Intrinsic::riscv_vredmin;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmin_vs_ta:
  ID = Intrinsic::riscv_vredmin;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmin_vs_tum:
  ID = Intrinsic::riscv_vredmin_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredmin_vs_tama:
case RISCVVector::BI__builtin_rvv_vredmin_vs_tam:
  ID = Intrinsic::riscv_vredmin_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredminu_vs_tu:
  ID = Intrinsic::riscv_vredminu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredminu_vs_ta:
  ID = Intrinsic::riscv_vredminu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredminu_vs_tum:
  ID = Intrinsic::riscv_vredminu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredminu_vs_tama:
case RISCVVector::BI__builtin_rvv_vredminu_vs_tam:
  ID = Intrinsic::riscv_vredminu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredor_vs_tu:
  ID = Intrinsic::riscv_vredor;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredor_vs_ta:
  ID = Intrinsic::riscv_vredor;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredor_vs_tum:
  ID = Intrinsic::riscv_vredor_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredor_vs_tama:
case RISCVVector::BI__builtin_rvv_vredor_vs_tam:
  ID = Intrinsic::riscv_vredor_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredsum_vs_tu:
  ID = Intrinsic::riscv_vredsum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredsum_vs_ta:
  ID = Intrinsic::riscv_vredsum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredsum_vs_tum:
  ID = Intrinsic::riscv_vredsum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredsum_vs_tama:
case RISCVVector::BI__builtin_rvv_vredsum_vs_tam:
  ID = Intrinsic::riscv_vredsum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredxor_vs_tu:
  ID = Intrinsic::riscv_vredxor;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredxor_vs_ta:
  ID = Intrinsic::riscv_vredxor;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredxor_vs_tum:
  ID = Intrinsic::riscv_vredxor_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vredxor_vs_tama:
case RISCVVector::BI__builtin_rvv_vredxor_vs_tam:
  ID = Intrinsic::riscv_vredxor_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_tu:
case RISCVVector::BI__builtin_rvv_vrem_vx_tu:
  ID = Intrinsic::riscv_vrem;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_ta:
case RISCVVector::BI__builtin_rvv_vrem_vx_ta:
  ID = Intrinsic::riscv_vrem;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_tuma:
case RISCVVector::BI__builtin_rvv_vrem_vx_tuma:
  ID = Intrinsic::riscv_vrem_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_tama:
case RISCVVector::BI__builtin_rvv_vrem_vx_tama:
  ID = Intrinsic::riscv_vrem_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_tumu:
case RISCVVector::BI__builtin_rvv_vrem_vx_tumu:
  ID = Intrinsic::riscv_vrem_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrem_vv_tamu:
case RISCVVector::BI__builtin_rvv_vrem_vx_tamu:
  ID = Intrinsic::riscv_vrem_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_tu:
case RISCVVector::BI__builtin_rvv_vremu_vx_tu:
  ID = Intrinsic::riscv_vremu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_ta:
case RISCVVector::BI__builtin_rvv_vremu_vx_ta:
  ID = Intrinsic::riscv_vremu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vremu_vx_tuma:
  ID = Intrinsic::riscv_vremu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_tama:
case RISCVVector::BI__builtin_rvv_vremu_vx_tama:
  ID = Intrinsic::riscv_vremu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vremu_vx_tumu:
  ID = Intrinsic::riscv_vremu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vremu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vremu_vx_tamu:
  ID = Intrinsic::riscv_vremu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_tu:
  ID = Intrinsic::riscv_vrgather_vv;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_ta:
  ID = Intrinsic::riscv_vrgather_vv;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_tuma:
  ID = Intrinsic::riscv_vrgather_vv_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_tama:
  ID = Intrinsic::riscv_vrgather_vv_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_tumu:
  ID = Intrinsic::riscv_vrgather_vv_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vv_tamu:
  ID = Intrinsic::riscv_vrgather_vv_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_tu:
  ID = Intrinsic::riscv_vrgather_vx;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_ta:
  ID = Intrinsic::riscv_vrgather_vx;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_tuma:
  ID = Intrinsic::riscv_vrgather_vx_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_tama:
  ID = Intrinsic::riscv_vrgather_vx_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_tumu:
  ID = Intrinsic::riscv_vrgather_vx_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgather_vx_tamu:
  ID = Intrinsic::riscv_vrgather_vx_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tu:
  ID = Intrinsic::riscv_vrgatherei16_vv;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_ta:
  ID = Intrinsic::riscv_vrgatherei16_vv;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tuma:
  ID = Intrinsic::riscv_vrgatherei16_vv_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tama:
  ID = Intrinsic::riscv_vrgatherei16_vv_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tumu:
  ID = Intrinsic::riscv_vrgatherei16_vv_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrgatherei16_vv_tamu:
  ID = Intrinsic::riscv_vrgatherei16_vv_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_tu:
  ID = Intrinsic::riscv_vrsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_tu:
  ID = Intrinsic::riscv_vrsub;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_ta:
  ID = Intrinsic::riscv_vrsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_ta:
  ID = Intrinsic::riscv_vrsub;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_tuma:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_tuma:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_tama:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_tama:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_tumu:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_tumu:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vrsub_vx_tamu:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vneg_v_tamu:
  ID = Intrinsic::riscv_vrsub_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));

        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType, ElemTy, Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType, ElemTy, Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vsadd_vx_tu:
  ID = Intrinsic::riscv_vsadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vsadd_vx_ta:
  ID = Intrinsic::riscv_vsadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsadd_vx_tuma:
  ID = Intrinsic::riscv_vsadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vsadd_vx_tama:
  ID = Intrinsic::riscv_vsadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsadd_vx_tumu:
  ID = Intrinsic::riscv_vsadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsadd_vx_tamu:
  ID = Intrinsic::riscv_vsadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_tu:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_tu:
  ID = Intrinsic::riscv_vsaddu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_ta:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_ta:
  ID = Intrinsic::riscv_vsaddu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_tuma:
  ID = Intrinsic::riscv_vsaddu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_tama:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_tama:
  ID = Intrinsic::riscv_vsaddu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_tumu:
  ID = Intrinsic::riscv_vsaddu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsaddu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsaddu_vx_tamu:
  ID = Intrinsic::riscv_vsaddu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsbc_vvm_tu:
case RISCVVector::BI__builtin_rvv_vsbc_vxm_tu:
  ID = Intrinsic::riscv_vsbc;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsbc_vvm_ta:
case RISCVVector::BI__builtin_rvv_vsbc_vxm_ta:
  ID = Intrinsic::riscv_vsbc;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vse8_v:
case RISCVVector::BI__builtin_rvv_vse16_v:
case RISCVVector::BI__builtin_rvv_vse32_v:
case RISCVVector::BI__builtin_rvv_vse64_v:
  ID = Intrinsic::riscv_vse;
  PolicyAttrs = 3;
IsMasked = false;

      if (IsMasked) {
        // Builtin: (mask, ptr, value, vl). Intrinsic: (value, ptr, mask, vl)
        std::swap(Ops[0], Ops[2]);
      } else {
        // Builtin: (ptr, value, vl). Intrinsic: (value, ptr, vl)
        std::swap(Ops[0], Ops[1]);
      }
      Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
      if (IsMasked)
        IntrinsicTypes = {Ops[0]->getType(), Ops[3]->getType()};
      else
        IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vse8_v_m:
case RISCVVector::BI__builtin_rvv_vse16_v_m:
case RISCVVector::BI__builtin_rvv_vse32_v_m:
case RISCVVector::BI__builtin_rvv_vse64_v_m:
  ID = Intrinsic::riscv_vse_mask;
  PolicyAttrs = 3;
IsMasked = true;

      if (IsMasked) {
        // Builtin: (mask, ptr, value, vl). Intrinsic: (value, ptr, mask, vl)
        std::swap(Ops[0], Ops[2]);
      } else {
        // Builtin: (ptr, value, vl). Intrinsic: (value, ptr, vl)
        std::swap(Ops[0], Ops[1]);
      }
      Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
      if (IsMasked)
        IntrinsicTypes = {Ops[0]->getType(), Ops[3]->getType()};
      else
        IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vsetvli:
  ID = Intrinsic::riscv_vsetvli;
  PolicyAttrs = 3;
IsMasked = false;
IntrinsicTypes = {ResultType};break;
case RISCVVector::BI__builtin_rvv_vsetvlimax:
  ID = Intrinsic::riscv_vsetvlimax;
  PolicyAttrs = 3;
IsMasked = false;
IntrinsicTypes = {ResultType};break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_tu:
case RISCVVector::BI__builtin_rvv_vsext_vf4_tu:
case RISCVVector::BI__builtin_rvv_vsext_vf8_tu:
  ID = Intrinsic::riscv_vsext;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_ta:
case RISCVVector::BI__builtin_rvv_vsext_vf4_ta:
case RISCVVector::BI__builtin_rvv_vsext_vf8_ta:
  ID = Intrinsic::riscv_vsext;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_tuma:
case RISCVVector::BI__builtin_rvv_vsext_vf4_tuma:
case RISCVVector::BI__builtin_rvv_vsext_vf8_tuma:
  ID = Intrinsic::riscv_vsext_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_tama:
case RISCVVector::BI__builtin_rvv_vsext_vf4_tama:
case RISCVVector::BI__builtin_rvv_vsext_vf8_tama:
  ID = Intrinsic::riscv_vsext_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_tumu:
case RISCVVector::BI__builtin_rvv_vsext_vf4_tumu:
case RISCVVector::BI__builtin_rvv_vsext_vf8_tumu:
  ID = Intrinsic::riscv_vsext_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsext_vf2_tamu:
case RISCVVector::BI__builtin_rvv_vsext_vf4_tamu:
case RISCVVector::BI__builtin_rvv_vsext_vf8_tamu:
  ID = Intrinsic::riscv_vsext_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_tu:
  ID = Intrinsic::riscv_vslide1down;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_ta:
  ID = Intrinsic::riscv_vslide1down;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_tuma:
  ID = Intrinsic::riscv_vslide1down_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_tama:
  ID = Intrinsic::riscv_vslide1down_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_tumu:
  ID = Intrinsic::riscv_vslide1down_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1down_vx_tamu:
  ID = Intrinsic::riscv_vslide1down_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_tu:
  ID = Intrinsic::riscv_vslide1up;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_ta:
  ID = Intrinsic::riscv_vslide1up;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_tuma:
  ID = Intrinsic::riscv_vslide1up_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_tama:
  ID = Intrinsic::riscv_vslide1up_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_tumu:
  ID = Intrinsic::riscv_vslide1up_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslide1up_vx_tamu:
  ID = Intrinsic::riscv_vslide1up_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_tu:
  ID = Intrinsic::riscv_vslidedown;
  PolicyAttrs = 2;
IsMasked = false;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_ta:
  ID = Intrinsic::riscv_vslidedown;
  PolicyAttrs = 3;
IsMasked = false;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_tuma:
  ID = Intrinsic::riscv_vslidedown_mask;
  PolicyAttrs = 2;
IsMasked = true;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_tama:
  ID = Intrinsic::riscv_vslidedown_mask;
  PolicyAttrs = 3;
IsMasked = true;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_tumu:
  ID = Intrinsic::riscv_vslidedown_mask;
  PolicyAttrs = 0;
IsMasked = true;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslidedown_vx_tamu:
  ID = Intrinsic::riscv_vslidedown_mask;
  PolicyAttrs = 1;
IsMasked = true;

      if (IsMasked) {
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      } else {
        if (PolicyAttrs & RVV_VTA)
          Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
      }

      Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
      IntrinsicTypes = {ResultType, Ops.back()->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_tu:
  ID = Intrinsic::riscv_vslideup;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_ta:
  ID = Intrinsic::riscv_vslideup;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_tuma:
  ID = Intrinsic::riscv_vslideup_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_tama:
  ID = Intrinsic::riscv_vslideup_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_tumu:
  ID = Intrinsic::riscv_vslideup_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vslideup_vx_tamu:
  ID = Intrinsic::riscv_vslideup_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_tu:
case RISCVVector::BI__builtin_rvv_vsll_vx_tu:
  ID = Intrinsic::riscv_vsll;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_ta:
case RISCVVector::BI__builtin_rvv_vsll_vx_ta:
  ID = Intrinsic::riscv_vsll;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsll_vx_tuma:
  ID = Intrinsic::riscv_vsll_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_tama:
case RISCVVector::BI__builtin_rvv_vsll_vx_tama:
  ID = Intrinsic::riscv_vsll_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsll_vx_tumu:
  ID = Intrinsic::riscv_vsll_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsll_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsll_vx_tamu:
  ID = Intrinsic::riscv_vsll_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsm_v:
  ID = Intrinsic::riscv_vsm;
  PolicyAttrs = 3;
IsMasked = false;

      if (IsMasked) {
        // Builtin: (mask, ptr, value, vl). Intrinsic: (value, ptr, mask, vl)
        std::swap(Ops[0], Ops[2]);
      } else {
        // Builtin: (ptr, value, vl). Intrinsic: (value, ptr, vl)
        std::swap(Ops[0], Ops[1]);
      }
      Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
      if (IsMasked)
        IntrinsicTypes = {Ops[0]->getType(), Ops[3]->getType()};
      else
        IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType()};
    break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_tu:
case RISCVVector::BI__builtin_rvv_vsmul_vx_tu:
  ID = Intrinsic::riscv_vsmul;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_ta:
case RISCVVector::BI__builtin_rvv_vsmul_vx_ta:
  ID = Intrinsic::riscv_vsmul;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsmul_vx_tuma:
  ID = Intrinsic::riscv_vsmul_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_tama:
case RISCVVector::BI__builtin_rvv_vsmul_vx_tama:
  ID = Intrinsic::riscv_vsmul_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsmul_vx_tumu:
  ID = Intrinsic::riscv_vsmul_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsmul_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsmul_vx_tamu:
  ID = Intrinsic::riscv_vsmul_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsoxei8_v:
case RISCVVector::BI__builtin_rvv_vsoxei16_v:
case RISCVVector::BI__builtin_rvv_vsoxei32_v:
case RISCVVector::BI__builtin_rvv_vsoxei64_v:
  ID = Intrinsic::riscv_vsoxei;
  PolicyAttrs = 3;
IsMasked = false;

        if (IsMasked) {
          // Builtin: (mask, ptr, index, value, vl). Intrinsic: (value, ptr, index, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, index, value, vl). Intrinsic: (value, ptr, index, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsoxei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxei64_v_m:
  ID = Intrinsic::riscv_vsoxei_mask;
  PolicyAttrs = 3;
IsMasked = true;

        if (IsMasked) {
          // Builtin: (mask, ptr, index, value, vl). Intrinsic: (value, ptr, index, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, index, value, vl). Intrinsic: (value, ptr, index, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsoxseg2ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei64_v:
  ID = Intrinsic::riscv_vsoxseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg2ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg2ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg3ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei64_v:
  ID = Intrinsic::riscv_vsoxseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg3ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg3ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg4ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei64_v:
  ID = Intrinsic::riscv_vsoxseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg4ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg4ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg5ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei64_v:
  ID = Intrinsic::riscv_vsoxseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg5ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg5ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg6ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei64_v:
  ID = Intrinsic::riscv_vsoxseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg6ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg6ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg7ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei64_v:
  ID = Intrinsic::riscv_vsoxseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg7ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg7ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg8ei8_v:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei16_v:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei32_v:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei64_v:
  ID = Intrinsic::riscv_vsoxseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsoxseg8ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsoxseg8ei64_v_m:
  ID = Intrinsic::riscv_vsoxseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsra_vv_tu:
case RISCVVector::BI__builtin_rvv_vsra_vx_tu:
  ID = Intrinsic::riscv_vsra;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsra_vv_ta:
case RISCVVector::BI__builtin_rvv_vsra_vx_ta:
  ID = Intrinsic::riscv_vsra;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsra_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsra_vx_tuma:
  ID = Intrinsic::riscv_vsra_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsra_vv_tama:
case RISCVVector::BI__builtin_rvv_vsra_vx_tama:
  ID = Intrinsic::riscv_vsra_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsra_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsra_vx_tumu:
  ID = Intrinsic::riscv_vsra_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsra_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsra_vx_tamu:
  ID = Intrinsic::riscv_vsra_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_tu:
case RISCVVector::BI__builtin_rvv_vsrl_vx_tu:
  ID = Intrinsic::riscv_vsrl;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_ta:
case RISCVVector::BI__builtin_rvv_vsrl_vx_ta:
  ID = Intrinsic::riscv_vsrl;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsrl_vx_tuma:
  ID = Intrinsic::riscv_vsrl_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_tama:
case RISCVVector::BI__builtin_rvv_vsrl_vx_tama:
  ID = Intrinsic::riscv_vsrl_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsrl_vx_tumu:
  ID = Intrinsic::riscv_vsrl_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsrl_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsrl_vx_tamu:
  ID = Intrinsic::riscv_vsrl_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsse16_v:
case RISCVVector::BI__builtin_rvv_vsse32_v:
case RISCVVector::BI__builtin_rvv_vsse64_v:
case RISCVVector::BI__builtin_rvv_vsse8_v:
  ID = Intrinsic::riscv_vsse;
  PolicyAttrs = 3;
IsMasked = false;

        if (IsMasked) {
          // Builtin: (mask, ptr, stride, value, vl). Intrinsic: (value, ptr, stride, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, stride, value, vl). Intrinsic: (value, ptr, stride, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsse16_v_m:
case RISCVVector::BI__builtin_rvv_vsse32_v_m:
case RISCVVector::BI__builtin_rvv_vsse64_v_m:
case RISCVVector::BI__builtin_rvv_vsse8_v_m:
  ID = Intrinsic::riscv_vsse_mask;
  PolicyAttrs = 3;
IsMasked = true;

        if (IsMasked) {
          // Builtin: (mask, ptr, stride, value, vl). Intrinsic: (value, ptr, stride, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, stride, value, vl). Intrinsic: (value, ptr, stride, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsseg2e16_v:
case RISCVVector::BI__builtin_rvv_vsseg2e32_v:
case RISCVVector::BI__builtin_rvv_vsseg2e64_v:
case RISCVVector::BI__builtin_rvv_vsseg2e8_v:
  ID = Intrinsic::riscv_vsseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg2e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg2e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg2e64_v_m:
case RISCVVector::BI__builtin_rvv_vsseg2e8_v_m:
  ID = Intrinsic::riscv_vsseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg3e8_v:
case RISCVVector::BI__builtin_rvv_vsseg3e16_v:
case RISCVVector::BI__builtin_rvv_vsseg3e32_v:
case RISCVVector::BI__builtin_rvv_vsseg3e64_v:
  ID = Intrinsic::riscv_vsseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg3e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg3e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg3e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg3e64_v_m:
  ID = Intrinsic::riscv_vsseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg4e8_v:
case RISCVVector::BI__builtin_rvv_vsseg4e16_v:
case RISCVVector::BI__builtin_rvv_vsseg4e32_v:
case RISCVVector::BI__builtin_rvv_vsseg4e64_v:
  ID = Intrinsic::riscv_vsseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg4e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg4e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg4e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg4e64_v_m:
  ID = Intrinsic::riscv_vsseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg5e8_v:
case RISCVVector::BI__builtin_rvv_vsseg5e16_v:
case RISCVVector::BI__builtin_rvv_vsseg5e32_v:
case RISCVVector::BI__builtin_rvv_vsseg5e64_v:
  ID = Intrinsic::riscv_vsseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg5e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg5e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg5e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg5e64_v_m:
  ID = Intrinsic::riscv_vsseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg6e8_v:
case RISCVVector::BI__builtin_rvv_vsseg6e16_v:
case RISCVVector::BI__builtin_rvv_vsseg6e32_v:
case RISCVVector::BI__builtin_rvv_vsseg6e64_v:
  ID = Intrinsic::riscv_vsseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg6e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg6e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg6e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg6e64_v_m:
  ID = Intrinsic::riscv_vsseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg7e8_v:
case RISCVVector::BI__builtin_rvv_vsseg7e16_v:
case RISCVVector::BI__builtin_rvv_vsseg7e32_v:
case RISCVVector::BI__builtin_rvv_vsseg7e64_v:
  ID = Intrinsic::riscv_vsseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg7e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg7e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg7e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg7e64_v_m:
  ID = Intrinsic::riscv_vsseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg8e8_v:
case RISCVVector::BI__builtin_rvv_vsseg8e16_v:
case RISCVVector::BI__builtin_rvv_vsseg8e32_v:
case RISCVVector::BI__builtin_rvv_vsseg8e64_v:
  ID = Intrinsic::riscv_vsseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsseg8e8_v_m:
case RISCVVector::BI__builtin_rvv_vsseg8e16_v_m:
case RISCVVector::BI__builtin_rvv_vsseg8e32_v_m:
case RISCVVector::BI__builtin_rvv_vsseg8e64_v_m:
  ID = Intrinsic::riscv_vsseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        std::swap(Ops[NF], Ops[NF + 1]);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      } else {
        // Builtin: (ptr, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, vl)
        std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
        assert(Ops.size() == NF + 2);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vssra_vv_tu:
case RISCVVector::BI__builtin_rvv_vssra_vx_tu:
  ID = Intrinsic::riscv_vssra;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssra_vv_ta:
case RISCVVector::BI__builtin_rvv_vssra_vx_ta:
  ID = Intrinsic::riscv_vssra;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssra_vv_tuma:
case RISCVVector::BI__builtin_rvv_vssra_vx_tuma:
  ID = Intrinsic::riscv_vssra_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssra_vv_tama:
case RISCVVector::BI__builtin_rvv_vssra_vx_tama:
  ID = Intrinsic::riscv_vssra_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssra_vv_tumu:
case RISCVVector::BI__builtin_rvv_vssra_vx_tumu:
  ID = Intrinsic::riscv_vssra_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssra_vv_tamu:
case RISCVVector::BI__builtin_rvv_vssra_vx_tamu:
  ID = Intrinsic::riscv_vssra_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_tu:
case RISCVVector::BI__builtin_rvv_vssrl_vx_tu:
  ID = Intrinsic::riscv_vssrl;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_ta:
case RISCVVector::BI__builtin_rvv_vssrl_vx_ta:
  ID = Intrinsic::riscv_vssrl;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_tuma:
case RISCVVector::BI__builtin_rvv_vssrl_vx_tuma:
  ID = Intrinsic::riscv_vssrl_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_tama:
case RISCVVector::BI__builtin_rvv_vssrl_vx_tama:
  ID = Intrinsic::riscv_vssrl_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_tumu:
case RISCVVector::BI__builtin_rvv_vssrl_vx_tumu:
  ID = Intrinsic::riscv_vssrl_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssrl_vv_tamu:
case RISCVVector::BI__builtin_rvv_vssrl_vx_tamu:
  ID = Intrinsic::riscv_vssrl_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssseg2e16_v:
case RISCVVector::BI__builtin_rvv_vssseg2e32_v:
case RISCVVector::BI__builtin_rvv_vssseg2e64_v:
case RISCVVector::BI__builtin_rvv_vssseg2e8_v:
  ID = Intrinsic::riscv_vssseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg2e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg2e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg2e64_v_m:
case RISCVVector::BI__builtin_rvv_vssseg2e8_v_m:
  ID = Intrinsic::riscv_vssseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg3e8_v:
case RISCVVector::BI__builtin_rvv_vssseg3e16_v:
case RISCVVector::BI__builtin_rvv_vssseg3e32_v:
case RISCVVector::BI__builtin_rvv_vssseg3e64_v:
  ID = Intrinsic::riscv_vssseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg3e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg3e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg3e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg3e64_v_m:
  ID = Intrinsic::riscv_vssseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg4e8_v:
case RISCVVector::BI__builtin_rvv_vssseg4e16_v:
case RISCVVector::BI__builtin_rvv_vssseg4e32_v:
case RISCVVector::BI__builtin_rvv_vssseg4e64_v:
  ID = Intrinsic::riscv_vssseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg4e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg4e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg4e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg4e64_v_m:
  ID = Intrinsic::riscv_vssseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg5e8_v:
case RISCVVector::BI__builtin_rvv_vssseg5e16_v:
case RISCVVector::BI__builtin_rvv_vssseg5e32_v:
case RISCVVector::BI__builtin_rvv_vssseg5e64_v:
  ID = Intrinsic::riscv_vssseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg5e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg5e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg5e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg5e64_v_m:
  ID = Intrinsic::riscv_vssseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg6e8_v:
case RISCVVector::BI__builtin_rvv_vssseg6e16_v:
case RISCVVector::BI__builtin_rvv_vssseg6e32_v:
case RISCVVector::BI__builtin_rvv_vssseg6e64_v:
  ID = Intrinsic::riscv_vssseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg6e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg6e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg6e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg6e64_v_m:
  ID = Intrinsic::riscv_vssseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg7e8_v:
case RISCVVector::BI__builtin_rvv_vssseg7e16_v:
case RISCVVector::BI__builtin_rvv_vssseg7e32_v:
case RISCVVector::BI__builtin_rvv_vssseg7e64_v:
  ID = Intrinsic::riscv_vssseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg7e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg7e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg7e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg7e64_v_m:
  ID = Intrinsic::riscv_vssseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg8e8_v:
case RISCVVector::BI__builtin_rvv_vssseg8e16_v:
case RISCVVector::BI__builtin_rvv_vssseg8e32_v:
case RISCVVector::BI__builtin_rvv_vssseg8e64_v:
  ID = Intrinsic::riscv_vssseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssseg8e8_v_m:
case RISCVVector::BI__builtin_rvv_vssseg8e16_v_m:
case RISCVVector::BI__builtin_rvv_vssseg8e32_v_m:
case RISCVVector::BI__builtin_rvv_vssseg8e64_v_m:
  ID = Intrinsic::riscv_vssseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, stride, val0, val1, ..., vl).
        // Intrinsic: (val0, val1, ..., ptr, stride, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        assert(Ops.size() == NF + 3);
      }
      IntrinsicTypes = {Ops[0]->getType(), Ops[NF + 1]->getType()};
    }
            break;
case RISCVVector::BI__builtin_rvv_vssub_vv_tu:
case RISCVVector::BI__builtin_rvv_vssub_vx_tu:
  ID = Intrinsic::riscv_vssub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssub_vv_ta:
case RISCVVector::BI__builtin_rvv_vssub_vx_ta:
  ID = Intrinsic::riscv_vssub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vssub_vx_tuma:
  ID = Intrinsic::riscv_vssub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssub_vv_tama:
case RISCVVector::BI__builtin_rvv_vssub_vx_tama:
  ID = Intrinsic::riscv_vssub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vssub_vx_tumu:
  ID = Intrinsic::riscv_vssub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vssub_vx_tamu:
  ID = Intrinsic::riscv_vssub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_tu:
case RISCVVector::BI__builtin_rvv_vssubu_vx_tu:
  ID = Intrinsic::riscv_vssubu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_ta:
case RISCVVector::BI__builtin_rvv_vssubu_vx_ta:
  ID = Intrinsic::riscv_vssubu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vssubu_vx_tuma:
  ID = Intrinsic::riscv_vssubu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_tama:
case RISCVVector::BI__builtin_rvv_vssubu_vx_tama:
  ID = Intrinsic::riscv_vssubu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vssubu_vx_tumu:
  ID = Intrinsic::riscv_vssubu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vssubu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vssubu_vx_tamu:
  ID = Intrinsic::riscv_vssubu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vsub_vx_tu:
  ID = Intrinsic::riscv_vsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vsub_vx_ta:
  ID = Intrinsic::riscv_vsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vsub_vx_tuma:
  ID = Intrinsic::riscv_vsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vsub_vx_tama:
  ID = Intrinsic::riscv_vsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vsub_vx_tumu:
  ID = Intrinsic::riscv_vsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vsub_vx_tamu:
  ID = Intrinsic::riscv_vsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vsuxei16_v:
case RISCVVector::BI__builtin_rvv_vsuxei32_v:
case RISCVVector::BI__builtin_rvv_vsuxei8_v:
case RISCVVector::BI__builtin_rvv_vsuxei64_v:
  ID = Intrinsic::riscv_vsuxei;
  PolicyAttrs = 3;
IsMasked = false;

        if (IsMasked) {
          // Builtin: (mask, ptr, index, value, vl). Intrinsic: (value, ptr, index, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, index, value, vl). Intrinsic: (value, ptr, index, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsuxei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxei64_v_m:
  ID = Intrinsic::riscv_vsuxei_mask;
  PolicyAttrs = 3;
IsMasked = true;

        if (IsMasked) {
          // Builtin: (mask, ptr, index, value, vl). Intrinsic: (value, ptr, index, mask, vl)
          std::swap(Ops[0], Ops[3]);
        } else {
          // Builtin: (ptr, index, value, vl). Intrinsic: (value, ptr, index, vl)
          std::rotate(Ops.begin(), Ops.begin() + 2, Ops.begin() + 3);
        }
        Ops[1] = Builder.CreateBitCast(Ops[1], Ops[0]->getType()->getPointerTo());
        if (IsMasked)
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[4]->getType()};
        else
          IntrinsicTypes = {Ops[0]->getType(), Ops[2]->getType(), Ops[3]->getType()};
      break;
case RISCVVector::BI__builtin_rvv_vsuxseg2ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei64_v:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei8_v:
  ID = Intrinsic::riscv_vsuxseg2;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg2ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei64_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg2ei8_v_m:
  ID = Intrinsic::riscv_vsuxseg2_mask;
  NF = 2;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg3ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei64_v:
  ID = Intrinsic::riscv_vsuxseg3;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg3ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg3ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg3_mask;
  NF = 3;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg4ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei64_v:
  ID = Intrinsic::riscv_vsuxseg4;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg4ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg4ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg4_mask;
  NF = 4;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg5ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei64_v:
  ID = Intrinsic::riscv_vsuxseg5;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg5ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg5ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg5_mask;
  NF = 5;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg6ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei64_v:
  ID = Intrinsic::riscv_vsuxseg6;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg6ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg6ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg6_mask;
  NF = 6;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg7ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei64_v:
  ID = Intrinsic::riscv_vsuxseg7;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg7ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg7ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg7_mask;
  NF = 7;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg8ei8_v:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei16_v:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei32_v:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei64_v:
  ID = Intrinsic::riscv_vsuxseg8;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = false;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vsuxseg8ei8_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei16_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei32_v_m:
case RISCVVector::BI__builtin_rvv_vsuxseg8ei64_v_m:
  ID = Intrinsic::riscv_vsuxseg8_mask;
  NF = 8;
  PolicyAttrs = 3;
IsMasked = true;

    {
      if (IsMasked) {
        // Builtin: (mask, ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, mask, vl)
        std::rotate(Ops.begin(), Ops.begin() + 3, Ops.end() - 1);
        std::rotate(Ops.begin() + NF, Ops.begin() + NF + 1, Ops.begin() + NF + 3);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 3]->getType()};
        assert(Ops.size() == NF + 4);
      } else {
        // Builtin: (ptr, index, val0, val1, ..., vl)
        // Intrinsic: (val0, val1, ..., ptr, index, vl)
        std::rotate(Ops.begin(), Ops.begin() + 2, Ops.end() - 1);
        IntrinsicTypes = {Ops[0]->getType(),
                          Ops[NF + 1]->getType(), Ops[NF + 2]->getType()};
        assert(Ops.size() == NF + 3);
      }
    }
            break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_tu:
case RISCVVector::BI__builtin_rvv_vwadd_vx_tu:
  ID = Intrinsic::riscv_vwadd;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_tu:
  ID = Intrinsic::riscv_vwadd;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_ta:
case RISCVVector::BI__builtin_rvv_vwadd_vx_ta:
  ID = Intrinsic::riscv_vwadd;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_ta:
  ID = Intrinsic::riscv_vwadd;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwadd_vx_tuma:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_tuma:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_tama:
case RISCVVector::BI__builtin_rvv_vwadd_vx_tama:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_tama:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwadd_vx_tumu:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_tumu:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwadd_vx_tamu:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvt_x_x_v_tamu:
  ID = Intrinsic::riscv_vwadd_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_tu:
case RISCVVector::BI__builtin_rvv_vwadd_wx_tu:
  ID = Intrinsic::riscv_vwadd_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_ta:
case RISCVVector::BI__builtin_rvv_vwadd_wx_ta:
  ID = Intrinsic::riscv_vwadd_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_tuma:
case RISCVVector::BI__builtin_rvv_vwadd_wx_tuma:
  ID = Intrinsic::riscv_vwadd_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_tama:
case RISCVVector::BI__builtin_rvv_vwadd_wx_tama:
  ID = Intrinsic::riscv_vwadd_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_tumu:
case RISCVVector::BI__builtin_rvv_vwadd_wx_tumu:
  ID = Intrinsic::riscv_vwadd_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwadd_wv_tamu:
case RISCVVector::BI__builtin_rvv_vwadd_wx_tamu:
  ID = Intrinsic::riscv_vwadd_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_tu:
  ID = Intrinsic::riscv_vwaddu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_tu:
  ID = Intrinsic::riscv_vwaddu;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_ta:
  ID = Intrinsic::riscv_vwaddu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_ta:
  ID = Intrinsic::riscv_vwaddu;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_tuma:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_tuma:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_tama:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_tama:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_tumu:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_tumu:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwaddu_vx_tamu:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwcvtu_x_x_v_tamu:
  ID = Intrinsic::riscv_vwaddu_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2, llvm::Constant::getNullValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, op2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passtru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            Ops[1]->getType(),
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_tu:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_tu:
  ID = Intrinsic::riscv_vwaddu_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_ta:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_ta:
  ID = Intrinsic::riscv_vwaddu_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_tuma:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_tuma:
  ID = Intrinsic::riscv_vwaddu_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_tama:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_tama:
  ID = Intrinsic::riscv_vwaddu_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_tumu:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_tumu:
  ID = Intrinsic::riscv_vwaddu_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwaddu_wx_tamu:
case RISCVVector::BI__builtin_rvv_vwaddu_wv_tamu:
  ID = Intrinsic::riscv_vwaddu_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_tu:
  ID = Intrinsic::riscv_vwmacc;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_ta:
  ID = Intrinsic::riscv_vwmacc;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_tuma:
  ID = Intrinsic::riscv_vwmacc_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_tama:
  ID = Intrinsic::riscv_vwmacc_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_tumu:
  ID = Intrinsic::riscv_vwmacc_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmacc_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmacc_vx_tamu:
  ID = Intrinsic::riscv_vwmacc_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_tu:
  ID = Intrinsic::riscv_vwmaccsu;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_ta:
  ID = Intrinsic::riscv_vwmaccsu;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_tuma:
  ID = Intrinsic::riscv_vwmaccsu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_tama:
  ID = Intrinsic::riscv_vwmaccsu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_tumu:
  ID = Intrinsic::riscv_vwmaccsu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccsu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmaccsu_vx_tamu:
  ID = Intrinsic::riscv_vwmaccsu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_tu:
  ID = Intrinsic::riscv_vwmaccu;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_ta:
  ID = Intrinsic::riscv_vwmaccu;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_tuma:
  ID = Intrinsic::riscv_vwmaccu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_tama:
  ID = Intrinsic::riscv_vwmaccu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_tumu:
  ID = Intrinsic::riscv_vwmaccu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmaccu_vx_tamu:
  ID = Intrinsic::riscv_vwmaccu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_tu:
  ID = Intrinsic::riscv_vwmaccus;
  PolicyAttrs = 2;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_ta:
  ID = Intrinsic::riscv_vwmaccus;
  PolicyAttrs = 3;
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_tuma:
  ID = Intrinsic::riscv_vwmaccus_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_tama:
  ID = Intrinsic::riscv_vwmaccus_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_tumu:
  ID = Intrinsic::riscv_vwmaccus_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmaccus_vx_tamu:
  ID = Intrinsic::riscv_vwmaccus_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmul_vx_tu:
  ID = Intrinsic::riscv_vwmul;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmul_vx_ta:
  ID = Intrinsic::riscv_vwmul;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmul_vx_tuma:
  ID = Intrinsic::riscv_vwmul_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmul_vx_tama:
  ID = Intrinsic::riscv_vwmul_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmul_vx_tumu:
  ID = Intrinsic::riscv_vwmul_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmul_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmul_vx_tamu:
  ID = Intrinsic::riscv_vwmul_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_tu:
  ID = Intrinsic::riscv_vwmulsu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_ta:
  ID = Intrinsic::riscv_vwmulsu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_tuma:
  ID = Intrinsic::riscv_vwmulsu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_tama:
  ID = Intrinsic::riscv_vwmulsu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_tumu:
  ID = Intrinsic::riscv_vwmulsu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulsu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmulsu_vx_tamu:
  ID = Intrinsic::riscv_vwmulsu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_tu:
  ID = Intrinsic::riscv_vwmulu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_ta:
  ID = Intrinsic::riscv_vwmulu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_tuma:
  ID = Intrinsic::riscv_vwmulu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_tama:
  ID = Intrinsic::riscv_vwmulu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_tumu:
  ID = Intrinsic::riscv_vwmulu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwmulu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwmulu_vx_tamu:
  ID = Intrinsic::riscv_vwmulu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsum_vs_tu:
  ID = Intrinsic::riscv_vwredsum;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsum_vs_ta:
  ID = Intrinsic::riscv_vwredsum;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsum_vs_tum:
  ID = Intrinsic::riscv_vwredsum_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsum_vs_tama:
case RISCVVector::BI__builtin_rvv_vwredsum_vs_tam:
  ID = Intrinsic::riscv_vwredsum_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsumu_vs_tu:
  ID = Intrinsic::riscv_vwredsumu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsumu_vs_ta:
  ID = Intrinsic::riscv_vwredsumu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsumu_vs_tum:
  ID = Intrinsic::riscv_vwredsumu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwredsumu_vs_tama:
case RISCVVector::BI__builtin_rvv_vwredsumu_vs_tam:
  ID = Intrinsic::riscv_vwredsumu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_tu:
case RISCVVector::BI__builtin_rvv_vwsub_vx_tu:
  ID = Intrinsic::riscv_vwsub;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_ta:
case RISCVVector::BI__builtin_rvv_vwsub_vx_ta:
  ID = Intrinsic::riscv_vwsub;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwsub_vx_tuma:
  ID = Intrinsic::riscv_vwsub_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_tama:
case RISCVVector::BI__builtin_rvv_vwsub_vx_tama:
  ID = Intrinsic::riscv_vwsub_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwsub_vx_tumu:
  ID = Intrinsic::riscv_vwsub_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwsub_vx_tamu:
  ID = Intrinsic::riscv_vwsub_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_tu:
case RISCVVector::BI__builtin_rvv_vwsub_wx_tu:
  ID = Intrinsic::riscv_vwsub_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_ta:
case RISCVVector::BI__builtin_rvv_vwsub_wx_ta:
  ID = Intrinsic::riscv_vwsub_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_tuma:
case RISCVVector::BI__builtin_rvv_vwsub_wx_tuma:
  ID = Intrinsic::riscv_vwsub_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_tama:
case RISCVVector::BI__builtin_rvv_vwsub_wx_tama:
  ID = Intrinsic::riscv_vwsub_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_tumu:
case RISCVVector::BI__builtin_rvv_vwsub_wx_tumu:
  ID = Intrinsic::riscv_vwsub_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsub_wv_tamu:
case RISCVVector::BI__builtin_rvv_vwsub_wx_tamu:
  ID = Intrinsic::riscv_vwsub_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_tu:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_tu:
  ID = Intrinsic::riscv_vwsubu;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_ta:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_ta:
  ID = Intrinsic::riscv_vwsubu;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_tuma:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_tuma:
  ID = Intrinsic::riscv_vwsubu_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_tama:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_tama:
  ID = Intrinsic::riscv_vwsubu_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_tumu:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_tumu:
  ID = Intrinsic::riscv_vwsubu_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_vv_tamu:
case RISCVVector::BI__builtin_rvv_vwsubu_vx_tamu:
  ID = Intrinsic::riscv_vwsubu_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_tu:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_tu:
  ID = Intrinsic::riscv_vwsubu_w;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_ta:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_ta:
  ID = Intrinsic::riscv_vwsubu_w;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_tuma:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_tuma:
  ID = Intrinsic::riscv_vwsubu_w_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_tama:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_tama:
  ID = Intrinsic::riscv_vwsubu_w_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_tumu:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_tumu:
  ID = Intrinsic::riscv_vwsubu_w_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vwsubu_wv_tamu:
case RISCVVector::BI__builtin_rvv_vwsubu_wx_tamu:
  ID = Intrinsic::riscv_vwsubu_w_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vxor_vv_tu:
case RISCVVector::BI__builtin_rvv_vxor_vx_tu:
  ID = Intrinsic::riscv_vxor;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_tu:
  ID = Intrinsic::riscv_vxor;
  PolicyAttrs = 2;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vxor_vv_ta:
case RISCVVector::BI__builtin_rvv_vxor_vx_ta:
  ID = Intrinsic::riscv_vxor;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_ta:
  ID = Intrinsic::riscv_vxor;
  PolicyAttrs = 3;
IsMasked = false;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vxor_vv_tuma:
case RISCVVector::BI__builtin_rvv_vxor_vx_tuma:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_tuma:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 2;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vxor_vv_tama:
case RISCVVector::BI__builtin_rvv_vxor_vx_tama:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_tama:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 3;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vxor_vv_tumu:
case RISCVVector::BI__builtin_rvv_vxor_vx_tumu:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_tumu:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 0;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vxor_vv_tamu:
case RISCVVector::BI__builtin_rvv_vxor_vx_tamu:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[2]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vnot_v_tamu:
  ID = Intrinsic::riscv_vxor_mask;
  PolicyAttrs = 1;
IsMasked = true;

      {
        if (IsMasked) {
          std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
          if ((PolicyAttrs & RVV_VTA) && (PolicyAttrs & RVV_VMA))
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        } else {
          if (PolicyAttrs & RVV_VTA)
            Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
        }
        auto ElemTy = cast<llvm::VectorType>(ResultType)->getElementType();
        Ops.insert(Ops.begin() + 2,
                   llvm::Constant::getAllOnesValue(ElemTy));
        if (IsMasked) {
          Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
          // maskedoff, op1, po2, mask, vl, policy
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[4]->getType()};
        } else {
          // passthru, op1, op2, vl
          IntrinsicTypes = {ResultType,
                            ElemTy,
                            Ops[3]->getType()};
        }
        break;
      }
      break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_tu:
case RISCVVector::BI__builtin_rvv_vzext_vf4_tu:
case RISCVVector::BI__builtin_rvv_vzext_vf8_tu:
  ID = Intrinsic::riscv_vzext;
  PolicyAttrs = 2;
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_ta:
case RISCVVector::BI__builtin_rvv_vzext_vf4_ta:
case RISCVVector::BI__builtin_rvv_vzext_vf8_ta:
  ID = Intrinsic::riscv_vzext;
  PolicyAttrs = 3;
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_tuma:
case RISCVVector::BI__builtin_rvv_vzext_vf4_tuma:
case RISCVVector::BI__builtin_rvv_vzext_vf8_tuma:
  ID = Intrinsic::riscv_vzext_mask;
  PolicyAttrs = 2;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_tama:
case RISCVVector::BI__builtin_rvv_vzext_vf4_tama:
case RISCVVector::BI__builtin_rvv_vzext_vf8_tama:
  ID = Intrinsic::riscv_vzext_mask;
  PolicyAttrs = 3;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  Ops.insert(Ops.begin(), llvm::PoisonValue::get(ResultType));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_tumu:
case RISCVVector::BI__builtin_rvv_vzext_vf4_tumu:
case RISCVVector::BI__builtin_rvv_vzext_vf8_tumu:
  ID = Intrinsic::riscv_vzext_mask;
  PolicyAttrs = 0;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;
case RISCVVector::BI__builtin_rvv_vzext_vf2_tamu:
case RISCVVector::BI__builtin_rvv_vzext_vf4_tamu:
case RISCVVector::BI__builtin_rvv_vzext_vf8_tamu:
  ID = Intrinsic::riscv_vzext_mask;
  PolicyAttrs = 1;
  std::rotate(Ops.begin(), Ops.begin() + 1, Ops.end() - 1);
  Ops.push_back(ConstantInt::get(Ops.back()->getType(), PolicyAttrs));
  IntrinsicTypes = {ResultType, Ops[1]->getType(), Ops.back()->getType()};
  break;

