package CAMP_HW1;

import java.io.*;
import java.util.*;

public class Calculator {

	public static void main(String[] args) throws IOException {
		int[] Register = new int[10];
		String str;
		String[] strNum = new String[20];
		int num1=0;
		int num2=0;
		int i = 0;
		int result = 0;
		
		BufferedReader rd = new BufferedReader(new FileReader("input2.txt"));
		while((str = rd.readLine()) != null ) {
			strNum[i] = str;
			i++;
		}
		
		for(i = 0;i < strNum.length;i++) {
			String[] operands = strNum[i].split(" ");
			
			if(operands.length == 3) {
				if(operands[1].charAt(0) == 'R') {
					num1 = Integer.parseInt(operands[1].replaceAll("[^0-9]", ""));
					operands[1] = Integer.toString(Register[num1], 16);
					if(operands[2].charAt(0) == 'R') {
						num2 = Integer.parseInt(operands[2].replaceAll("[^0-9]", ""));
						operands[2] = Integer.toString(Register[num2], 16);
						result = 0;
					}else {
						operands[2] = operands[2].replaceFirst("^0x", "");
						result = num1;
					}
				}else {
					operands[1] = operands[1].replaceFirst("^0x", "");
					operands[2] = operands[2].replaceFirst("^0x", "");
					result = 0;
				}
			}
			
			switch(operands[0]) {
			case "+":
				Register[result] = Integer.parseInt(operands[1], 16) + Integer.parseInt(operands[2], 16);
				break;
			case "-":
				Register[result] = Integer.parseInt(operands[1], 16) - Integer.parseInt(operands[2], 16);
				break;
			case "*":
				Register[result] = Integer.parseInt(operands[1], 16) * Integer.parseInt(operands[2], 16);
				break;
			case "/":
				Register[result] = Integer.parseInt(operands[1], 16) / Integer.parseInt(operands[2], 16);
				break;
			case "M":
				Register[num1] = Integer.parseInt(operands[2], 16);
				break;
			case "C":
				if(Integer.parseInt(operands[1], 16) < Integer.parseInt(operands[2], 16)) {
					Register[0] = 1;
				}else {
					Register[0] = 0;
				}
				break;
			case "B":
				if(Register[0] == 1) {
					operands[1] = operands[1].replaceFirst("^0x", "");
					i = Integer.parseInt(operands[1], 16) - 1;
				}
				break;
			case "J":
				i = Integer.parseInt(operands[1], 16);
				break;
			case "H":
				System.out.println(operands[0]);
				System.exit(0);
				break;
			}
			
			if(operands.length == 3) {
				if(result != 0) {
					System.out.println("R"+result+": "+Register[result]);
				}else {
					System.out.println("R"+result+": "+Register[result]+" = "+operands[1]+" "+operands[0]+" "+operands[2]);
				}
			}else {
				System.out.println(operands[0]);
			}
		}
		
	}

}
