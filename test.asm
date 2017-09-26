start:	SET r2, #0
		SET r3, #1
		SET r10,  #x1000
		SET r11, #x10000
		MUL r10, r10, r11
		SET r11, #4
		ADD r10, r10, r11

		SET r11, #x000FFFFF
		SET r12, #8
loop:	ADD r4, r2, r3
		AND r2, r3, r3
		AND r3, r4, r4

		CMP r3, r11
		IFME
			ADD r0, r0, r12

		STR r3, r10
		SET r0, #32

end:	SET r11, #4
		SUB r0, r0, r11
