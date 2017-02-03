import java.util.*;
import java.io.*;
class Parser
{
	public static void main(String [] args) throws FileNotFoundException
	{
		try {
			System.out.println(new File("ExperimentResults.csv ").getAbsoluteFile());
			BufferedReader input = new BufferedReader(new FileReader("ExperimentResults.csv"));
			String data;
			data = input.readLine();
			float sum = 0;
			System.out.println("Reading File........");
			while((data = input.readLine()) != null)
			{
				String [] splitted = data.split(",");
				if(splitted.length > 0)
					System.out.println(splitted[0]);
				else System.out.println("Not split properly");
				sum = sum + Float.parseFloat(splitted[splitted.length-1]);	
			}
			sum /=10;
			System.out.println(sum); 
		}
		catch (Exception e) {
			System.out.println("File not found");
		}
	}		
		
}
