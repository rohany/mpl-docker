(*
 * The interface to the new register allocator.
 *
 * -- Allen
 *)
signature RA =
sig

   structure I : INSTRUCTIONS
   structure C : CELLS
   structure F : RA_FLOWGRAPH 
      (* sharing F.I = I *)
     where type I.addressing_mode = I.addressing_mode
       and type I.ea = I.ea
       and type I.instr = I.instr
       and type I.instruction = I.instruction
       and type I.operand = I.operand
      (* sharing I.C = C *)
   structure CB : CELLS_BASIS (* = CellsBasis *)
                  where type CellSet.cellset = CellsBasis.CellSet.cellset
                    and type 'a ColorTable.hash_table = 'a CellsBasis.ColorTable.hash_table
                    and type 'a HashTable.hash_table = 'a CellsBasis.HashTable.hash_table
                    and type SortedCells.sorted_cells = CellsBasis.SortedCells.sorted_cells
                    and type cell = CellsBasis.cell
                    and type cellColor = CellsBasis.cellColor
                    and type cellkind = CellsBasis.cellkind
                    and type cellkindDesc = CellsBasis.cellkindDesc
                    and type cellkindInfo = CellsBasis.cellkindInfo

   type getreg = { pref  : CB.cell_id list,
                   stamp : int, 
                   proh  : int Array.array
                 } -> CB.cell_id

   type mode = word

   datatype spillLoc = datatype RAGraph.spillLoc

   (*
    * Optimizations/options:
    * Or them together
    *)
   val NO_OPTIMIZATION      : mode
   val DEAD_COPY_ELIM       : mode
   val BIASED_SELECTION     : mode
   val SPILL_COLORING       : mode
   val SPILL_COALESCING     : mode
   val SPILL_PROPAGATION    : mode
   val HAS_PARALLEL_COPIES  : mode 
       (* The above MUST be used when spill coloring is used and
        * you have parallel copies in the program. Otherwise, phathom
        * problems involving copy temporaries may appear.
        *)

   (*
    * Perform register allocation.
    *
    * spillProh is a list of register ranges (inclusive) that cannot be spilled.
    *
    *)
   type raClient = 
   { cellkind     : CB.cellkind,             (* kind of register *)
     spillProh    : CB.cell list,            (* don't spill these *)
     memRegs      : CB.cell list,            (* memory registers *)
     K            : int,                    (* number of colors *)
     dedicated    : int -> bool,            (* dedicated registers *)
     getreg       : getreg,                 (* how to find a color *)
     copyInstr    : F.Spill.copyInstr,      (* how to make a copy *)
     spill        : F.Spill.spill,          (* spill callback *)
     spillSrc     : F.Spill.spillSrc,       (* spill callback *)
     spillCopyTmp : F.Spill.spillCopyTmp,   (* spill callback *)
     reload       : F.Spill.reload,         (* reload callback *)
     reloadDst    : F.Spill.reloadDst,      (* reload callback *)
     renameSrc    : F.Spill.renameSrc,      (* rename callback *)
     mode         : mode                    (* mode *)
   } 

   val ra : raClient list -> F.flowgraph -> F.flowgraph

end
