var sheet_id = 'Replace with Speadsheet ID'; 		// Spreadsheet ID

function doGet(e) { 
  Logger.log( JSON.stringify(e) ); 
  var result = 'Ok\n';
  if (!e) {
    result = '';
  }
  else {
    var sheet = SpreadsheetApp.openById(sheet_id).getActiveSheet();		// Get active sheet

    if (!sheet)
    {
      Logger.log("Sheet\"" + tabName + "\" not found.");
      return ContentService.createTextOutput("Sheet \"" + tabName + "\" not found.");
    }

    if(sheet.getLastRow() !== 1)
      deleteOldRows();
    var newRow = sheet.getLastRow() + 1;						
    var rowData = [];
    
    // Going through all the parameters and putting them into the proper location
    for (var param in e.parameter) {
      Logger.log('In for loop, param=' + param);
      var value = stripQuotes(e.parameter[param]);
      Logger.log(param + ':' + e.parameter[param]);
      switch (param) {
        case 'date':
          rowData[0] = value; // Convert the datetime string to a Date object
          // Get the range of the cell to format
          result += 'Written Time and Date on Column 0\n';
          break;
        case 'time':
          rowData[1] = value; // Convert the datetime string to a Date object
          // Get the range of the cell to format
          result += 'Written Time and Date on Column 1\n';
          break;
        case 'temp': //Parameter 1, It has to be updated in Column in Sheets in the code, orderwise
          rowData[2] = value; //Value in column A
          result += 'Written on column A\n';
          break;
        case 'humidity': //Parameter 2, It has to be updated in Column in Sheets in the code, orderwise
          rowData[3] = value; //Value in column B
          result += 'Written on column B\n';
          break;
        case 'co2': //Parameter 3, It has to be updated in Column in Sheets in the code, orderwise
          rowData[4] = value; //Value in column C
          result += 'Written on column C\n';
          break;
        case 'so2': //Parameter 4, It has to be updated in Column in Sheets in the code, orderwise
          rowData[5] = value; //Value in column D
          result += 'Written on column D\n';
          break;
        case 'hcho': //Parameter 5, It has to be updated in Column in Sheets in the code, orderwise
          rowData[6] = value; //Value in column E
          result += 'Written on column E\n';
          break;
        case 'ph3': //Parameter 5, It has to be updated in Column in Sheets in the code, orderwise
          rowData[7] = value; //Value in column E
          result += 'Written on column F\n';
          break;
        default:
          result = "unsupported parameter";
      }
    }
    Logger.log(JSON.stringify(rowData));
    // Write new row below
    var newRange = sheet.getRange(newRow, 1, 1, rowData.length);
    newRange.setValues([rowData]);
  }
  // Add new empty row after
  var sheet = SpreadsheetApp.openById(sheet_id).getActiveSheet();
  sheet.insertRowsAfter(sheet.getLastRow(), 1);
  // Return result of operation
  return ContentService.createTextOutput(result);
}

function stripQuotes( value ) {
  return value.replace(/^["']|['"]$/g, "");
}

// Deleting data older than a month and empty rows
function deleteOldRows() {
  const sheet = SpreadsheetApp.openById(sheet_id).getActiveSheet();
  const dateColumnIndex = 1; // The index for column A
  const headerRows = 1; // The number of header rows

  const dateThreshold = new Date();
  dateThreshold.setMonth(dateThreshold.getMonth() - 1); // Set the threshold date to one month before today

  const lastRow = sheet.getLastRow();
  const dateValues = sheet.getRange(headerRows + 1, dateColumnIndex, lastRow - headerRows, 1).getValues();

  let rowsToDelete = [];

  // Loop through the date values from bottom to top
  for (let i = dateValues.length - 1; i >= 0; i--) {
    const rowDate = new Date(dateValues[i][0]);
    if (rowDate < dateThreshold || !dateValues[i][0]) {
      rowsToDelete.push(i + headerRows + 1); // Add the row index to the list of rows to delete if the date is older than the threshold or the row is empty
    }
  }

  // Delete all rows at once
  if (rowsToDelete.length > 0) {
    sheet.deleteRows(2, rowsToDelete.length);
  }
}
